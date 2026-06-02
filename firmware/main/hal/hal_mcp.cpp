/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include <mooncake_log.h>
#include <mcp_server.h>
#include <stackchan/stackchan.h>
#include <apps/common/common.h>
#if CONFIG_HAL_SCD41_ENABLED || CONFIG_HAL_ENVIII_ENABLED
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

using namespace stackchan;

static const std::string_view _tag = "HAL-MCP";

#if CONFIG_HAL_SCD41_ENABLED
static Hal::Scd41Data _mcp_scd41_cache;
static SemaphoreHandle_t _scd41_cache_mutex = nullptr;
#endif

#if CONFIG_HAL_ENVIII_ENABLED
static Hal::EnvIIIData _mcp_enviii_cache;
static SemaphoreHandle_t _enviii_cache_mutex = nullptr;
#endif

void Hal::xiaozhi_mcp_init()
{
    mclog::tagInfo(_tag, "init");

#if CONFIG_HAL_SCD41_ENABLED
    _scd41_cache_mutex = xSemaphoreCreateMutex();

    onScd41Measurement.connect([](const Scd41Data& data) {
        if (_scd41_cache_mutex) {
            xSemaphoreTake(_scd41_cache_mutex, portMAX_DELAY);
            _mcp_scd41_cache = data;
            xSemaphoreGive(_scd41_cache_mutex);
        }
    });
#endif

#if CONFIG_HAL_ENVIII_ENABLED
    _enviii_cache_mutex = xSemaphoreCreateMutex();

    onEnvIIIMeasurement.connect([](const EnvIIIData& data) {
        if (_enviii_cache_mutex) {
            xSemaphoreTake(_enviii_cache_mutex, portMAX_DELAY);
            _mcp_enviii_cache = data;
            xSemaphoreGive(_enviii_cache_mutex);
        }
    });
#endif

    // https://github.com/78/xiaozhi-esp32/blob/main/docs/mcp-usage.md
    auto& mcp_server = McpServer::GetInstance();

    // System Prompt：
    // You can control the robot's head. Use get_yaw and get_pitch to sense current position. Use set_yaw for horizontal
    // movement and set_pitch for vertical movement. All angles are in degrees.

    mclog::tagInfo(_tag, "add robot.get_head_angles tool");
    mcp_server.AddTool("self.robot.get_head_angles",
                       "Returns current yaw/pitch in degrees. Neutral position is {yaw:0, pitch:0}.",
                       std::vector<Property>{}, [this](const PropertyList& properties) -> ReturnValue {
                           LvglLockGuard lock;  // StackChan motion update is under the lvgl lock

                           auto& motion      = GetStackChan().motion();
                           int current_yaw   = motion.yawServo().getCurrentAngle() / 10;
                           int current_pitch = motion.pitchServo().getCurrentAngle() / 10;

                           auto result = fmt::format(R"({{"yaw": {}, "pitch": {}}})", current_yaw, current_pitch);
                           mclog::tagInfo(_tag, "get_head_angles: {}", result);
                           return result;
                       });

    mclog::tagInfo(_tag, "add robot.set_head_angles tool");
    mcp_server.AddTool("self.robot.set_head_angles",
                       "Adjust head position. GUIDELINES: "
                       "1. For natural interaction, stay within +/- 45 degrees. "
                       "2. Only use values > 70 if the user explicitly asks to look far away/behind. "
                       "3. Max ranges: Yaw(-128 to 128, -128 as your left), Pitch(0 to 90, 90 as your up). "
                       "Speed(100-1000, 150 is natural).",
                       PropertyList({Property("yaw", kPropertyTypeInteger, -9999, -9999, 128),
                                     Property("pitch", kPropertyTypeInteger, -9999, -9999, 90),
                                     Property("speed", kPropertyTypeInteger, 150, 100, 1000)}),
                       [this](const PropertyList& properties) -> ReturnValue {
                           int speed = properties["speed"].value<int>();
                           int yaw   = properties["yaw"].value<int>();
                           int pitch = properties["pitch"].value<int>();

                           mclog::tagInfo(_tag, "motion set_angles: yaw: {}, pitch: {}, speed: {}", yaw, pitch, speed);

                           LvglLockGuard lock;

                           auto& motion = GetStackChan().motion();
                           if (pitch != -9999) {
                               motion.pitchServo().moveWithSpeed(pitch * 10, speed);
                           }
                           if (yaw != -9999) {
                               motion.yawServo().moveWithSpeed(yaw * 10, speed);
                           }

                           return true;
                       });

    mclog::tagInfo(_tag, "add robot.set_led_color tool");
    mcp_server.AddTool(
        "self.robot.set_led_color",
        "Set the color of the robot's INTERNAL onboard LED. This is NOT for room lights. "
        "Values: 0-168 (safe range). Red=168,0,0; Green=0,168,0; Blue=0,0,168; White=100,100,100; Off=0,0,0.",
        PropertyList({Property("red", kPropertyTypeInteger, 0, 0, 168),
                      Property("green", kPropertyTypeInteger, 0, 0, 168),
                      Property("blue", kPropertyTypeInteger, 0, 0, 168)}),
        [this](const PropertyList& properties) -> ReturnValue {
            int r = properties["red"].value<int>();
            int g = properties["green"].value<int>();
            int b = properties["blue"].value<int>();

            mclog::tagInfo(_tag, "set_led_color: r={}, g={}, b={}", r, g, b);

            LvglLockGuard lock;

            GetStackChan().leftNeonLight().setColor(r, g, b);
            GetStackChan().rightNeonLight().setColor(r, g, b);

            return true;
        });

    mclog::tagInfo(_tag, "add robot.create_reminder tool");
    mcp_server.AddTool("self.robot.create_reminder",
                       "Create a reminder. Duration is in seconds. Message is what to say when time is up. Set repeat "
                       "to true to repeat the reminder.",
                       PropertyList({Property("duration_seconds", kPropertyTypeInteger, 60, 1, 86400),
                                     Property("message", kPropertyTypeString, std::string("Time's up!")),
                                     Property("repeat", kPropertyTypeBoolean, false)}),
                       [this](const PropertyList& properties) -> ReturnValue {
                           int duration_seconds = properties["duration_seconds"].value<int>();
                           std::string message  = properties["message"].value<std::string>();
                           bool repeat          = properties["repeat"].value<bool>();

                           // Default message
                           if (message.empty()) {
                               message = "Time's up!";
                           }

                           mclog::tagInfo(_tag, "create_reminder: duration={}s, message={}, repeat={}",
                                          duration_seconds, message, repeat);

                           int id = tools::create_reminder(duration_seconds * 1000, message, repeat);

                           return id;
                       });

    mclog::tagInfo(_tag, "add robot.get_reminders tool");
    mcp_server.AddTool("self.robot.get_reminders", "Get list of active reminders.", std::vector<Property>{},
                       [this](const PropertyList& properties) -> ReturnValue {
                           mclog::tagInfo(_tag, "get_reminders");
                           auto reminders          = tools::get_active_reminders();
                           std::string result_json = "[";
                           for (size_t i = 0; i < reminders.size(); ++i) {
                               const auto& r = reminders[i];
                               result_json +=
                                   fmt::format(R"({{"id": {}, "duration_ms": {}, "message": "{}", "repeat": {}}})",
                                               r.id, r.durationMs, r.message, r.repeat ? "true" : "false");
                               if (i < reminders.size() - 1) {
                                   result_json += ", ";
                               }
                           }
                           result_json += "]";
                           mclog::tagInfo(_tag, "get_reminders result: {}", result_json);
                           return result_json;
                       });

    mclog::tagInfo(_tag, "add robot.stop_reminder tool");
    mcp_server.AddTool("self.robot.stop_reminder", "Stop a reminder by ID.",
                       PropertyList({Property("id", kPropertyTypeInteger, -1)}),
                       [this](const PropertyList& properties) -> ReturnValue {
                           int id = properties["id"].value<int>();
                           mclog::tagInfo(_tag, "stop_reminder: id={}", id);
                           tools::stop_reminder(id);
                           return true;
                       });

#if CONFIG_HAL_SCD41_ENABLED
    mclog::tagInfo(_tag, "add robot.get_co2_data tool");
    mcp_server.AddTool(
        "self.robot.get_co2_data",
        "Returns current air quality data from the SCD41 CO2 sensor. "
        "Call this tool when asked about air, ventilation, temperature, humidity, or CO2, then answer concisely based on the result. "
        "co2_ppm: CO2 concentration in parts per million (400-2000 typical indoor range, >1000 suggests ventilation needed). "
        "temperature_c: ambient temperature in Celsius. "
        "humidity_percent: relative humidity 0-100%. "
        "Returns an error field if the sensor is not connected.",
        std::vector<Property>{},
        [](const PropertyList& properties) -> ReturnValue {
            Hal::Scd41Data snapshot;
            if (_scd41_cache_mutex) {
                xSemaphoreTake(_scd41_cache_mutex, portMAX_DELAY);
                snapshot = _mcp_scd41_cache;
                xSemaphoreGive(_scd41_cache_mutex);
            }

            if (snapshot.co2_ppm == 0) {
                mclog::tagInfo(_tag, "get_co2_data: sensor not available");
                return std::string(R"({"error": "SCD41 not available"})");
            }

            auto result = fmt::format(
                R"({{"co2_ppm": {}, "temperature_c": {:.1f}, "humidity_percent": {:.1f}}})",
                snapshot.co2_ppm, snapshot.temperature_c, snapshot.humidity_percent);
            mclog::tagInfo(_tag, "get_co2_data: {}", result);
            return result;
        });
#endif // CONFIG_HAL_SCD41_ENABLED

#if CONFIG_HAL_ENVIII_ENABLED
    mclog::tagInfo(_tag, "add robot.get_environment_data tool");
    mcp_server.AddTool(
        "self.robot.get_environment_data",
        "Returns current environment data from the M5Stack ENV III sensor on PORT.A. "
        "Call this tool when asked about room temperature, humidity, atmospheric pressure, or altitude. "
        "temperature_c: ambient temperature in Celsius. "
        "humidity_percent: relative humidity 0-100%. "
        "pressure_hpa: atmospheric pressure in hectopascals. "
        "altitude_m: estimated altitude in meters from pressure. "
        "Returns an error field if the sensor is not connected.",
        std::vector<Property>{},
        [](const PropertyList& properties) -> ReturnValue {
            Hal::EnvIIIData snapshot;
            if (_enviii_cache_mutex) {
                xSemaphoreTake(_enviii_cache_mutex, portMAX_DELAY);
                snapshot = _mcp_enviii_cache;
                xSemaphoreGive(_enviii_cache_mutex);
            }

            if (snapshot.pressure_hpa <= 0.0f) {
                mclog::tagInfo(_tag, "get_environment_data: sensor not available");
                return std::string(R"({"error": "ENV III not available"})");
            }

            auto result = fmt::format(
                R"({{"temperature_c": {:.1f}, "humidity_percent": {:.1f}, "pressure_hpa": {:.1f}, "altitude_m": {:.1f}}})",
                snapshot.temperature_c, snapshot.humidity_percent, snapshot.pressure_hpa, snapshot.altitude_m);
            mclog::tagInfo(_tag, "get_environment_data: {}", result);
            return result;
        });
#endif // CONFIG_HAL_ENVIII_ENABLED
}
