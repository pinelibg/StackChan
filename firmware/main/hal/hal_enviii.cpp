/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#if CONFIG_HAL_ENVIII_ENABLED
#include "board/hal_bridge.h"
#include "drivers/enviii/enviii.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <memory>
#include <mooncake_log.h>

static const std::string_view _tag = "HAL-ENVIII";

static std::unique_ptr<EnvIII> _enviii;

static void _enviii_task(void* param)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (!_enviii) {
            continue;
        }

        EnvIIIMeasurement measurement;
        if (_enviii->readMeasurement(measurement) != ESP_OK) {
            continue;
        }

        GetHAL().onEnvIIIMeasurement.emit(Hal::EnvIIIData{
            .temperature_c = measurement.temperature_c,
            .humidity_percent = measurement.humidity_percent,
            .pressure_hpa = measurement.pressure_hpa,
            .pressure_pa = measurement.pressure_pa,
            .altitude_m = measurement.altitude_m,
        });
    }
}

void Hal::enviii_init()
{
    mclog::tagInfo(_tag, "init");

    auto i2c_bus = hal_bridge::board_get_port_a_i2c_bus();
    if (!i2c_bus) {
        mclog::tagError(_tag, "port A I2C bus not available");
        return;
    }

    _enviii = std::make_unique<EnvIII>(i2c_bus);
    if (_enviii->begin() != ESP_OK) {
        _enviii.reset();
        mclog::tagError(_tag, "ENV III init failed");
        return;
    }

    mclog::tagInfo(_tag, "ENV III init ok");

    xTaskCreatePinnedToCoreWithCaps(_enviii_task, "enviii", 4096, NULL, 2, NULL, 1, MALLOC_CAP_SPIRAM);
}

#endif // CONFIG_HAL_ENVIII_ENABLED
