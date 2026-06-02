/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "sdkconfig.h"
#include "app_enviii.h"

#if CONFIG_HAL_ENVIII_ENABLED

#include <apps/common/common.h>
#include <fmt/format.h>
#include <mooncake_log.h>

using namespace smooth_ui_toolkit::lvgl_cpp;

namespace {
uint32_t temperatureColor(float temperature_c)
{
    if (temperature_c >= 30.0f) {
        return 0xDC2626;
    }
    if (temperature_c <= 15.0f) {
        return 0x2563EB;
    }
    return 0x16A34A;
}
}  // namespace

AppEnvIII::AppEnvIII()
{
    setAppInfo().name = "ENV III";
    static uint32_t theme_color = 0x3A8F7A;
    setAppInfo().userData = (void*)&theme_color;
}

void AppEnvIII::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppEnvIII::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    {
        LvglLockGuard lock;
        createView();
        updateStatus("Initializing PORT.A sensor...");
        view::create_home_indicator([&]() { close(); }, 0x9AD8C9, 0x123D35);
        view::create_status_bar(0x9AD8C9, 0x123D35);
    }

    _enviii_conn_id = GetHAL().onEnvIIIMeasurement.connect([this](const Hal::EnvIIIData& data) {
        LvglLockGuard lock;
        updateMeasurement(data);
    });

    LvglLockGuard lock;
    updateStatus("Waiting for first measurement...");
}

void AppEnvIII::onRunning()
{
    LvglLockGuard lock;
    view::update_home_indicator();
    view::update_status_bar();
}

void AppEnvIII::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    GetHAL().onEnvIIIMeasurement.disconnect(_enviii_conn_id);
    _enviii_conn_id = -1;

    LvglLockGuard lock;
    _title.reset();
    _sensor_name_label.reset();
    _status_pill.reset();
    _pulse_dot.reset();
    _update_label.reset();
    _temp_caption.reset();
    _temp_label.reset();
    _temp_card.reset();
    _humidity_caption.reset();
    _humidity_label.reset();
    _humidity_card.reset();
    _pressure_caption.reset();
    _pressure_label.reset();
    _pressure_card.reset();
    _altitude_caption.reset();
    _altitude_label.reset();
    _altitude_card.reset();
    _status_label.reset();
    _panel.reset();
    _update_count = 0;
    _pulse_on = false;

    view::destroy_home_indicator();
    view::destroy_status_bar();
}

void AppEnvIII::createView()
{
    _panel = std::make_unique<Container>(lv_screen_active());
    _panel->setSize(320, 240);
    _panel->setAlign(LV_ALIGN_CENTER);
    _panel->setBgColor(lv_color_hex(0xF7FAFC));
    _panel->setBorderWidth(0);
    _panel->setRadius(0);
    _panel->setPaddingAll(0);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _title = std::make_unique<Label>(*_panel);
    _title->setText("M5Stack ENV III");
    _title->setTextFont(&lv_font_montserrat_20);
    _title->setTextColor(lv_color_hex(0x102A43));
    _title->align(LV_ALIGN_TOP_LEFT, 16, 30);

    _sensor_name_label = std::make_unique<Label>(*_panel);
    _sensor_name_label->setText("Temp / RH / Pressure");
    _sensor_name_label->setTextFont(&lv_font_montserrat_14);
    _sensor_name_label->setTextColor(lv_color_hex(0x52616B));
    _sensor_name_label->align(LV_ALIGN_TOP_LEFT, 17, 54);

    _status_pill = std::make_unique<Container>(*_panel);
    _status_pill->setSize(82, 26);
    _status_pill->setBgColor(lv_color_hex(0xE7F6F1));
    _status_pill->setBorderColor(lv_color_hex(0x9AD8C9));
    _status_pill->setBorderWidth(2);
    _status_pill->setRadius(8);
    _status_pill->setPaddingAll(0);
    _status_pill->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _status_pill->align(LV_ALIGN_TOP_RIGHT, -14, 35);

    _pulse_dot = std::make_unique<Container>(*_status_pill);
    _pulse_dot->setSize(10, 10);
    _pulse_dot->setBgColor(lv_color_hex(0x94A3B8));
    _pulse_dot->setBorderWidth(0);
    _pulse_dot->setRadius(LV_RADIUS_CIRCLE);
    _pulse_dot->align(LV_ALIGN_LEFT_MID, 10, 0);

    _update_label = std::make_unique<Label>(*_status_pill);
    _update_label->setText("#0");
    _update_label->setTextFont(&lv_font_montserrat_14);
    _update_label->setTextColor(lv_color_hex(0x0F766E));
    _update_label->setWidth(52);
    _update_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _update_label->align(LV_ALIGN_RIGHT_MID, -4, 0);

    _temp_card = std::make_unique<Container>(*_panel);
    _temp_card->setSize(137, 62);
    _temp_card->setBgColor(lv_color_hex(0xFFFFFF));
    _temp_card->setBorderColor(lv_color_hex(0xFED7AA));
    _temp_card->setBorderWidth(2);
    _temp_card->setRadius(8);
    _temp_card->setPaddingAll(0);
    _temp_card->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _temp_card->align(LV_ALIGN_TOP_LEFT, 16, 82);

    _temp_caption = std::make_unique<Label>(*_temp_card);
    _temp_caption->setText("TEMP");
    _temp_caption->setTextFont(&lv_font_montserrat_14);
    _temp_caption->setTextColor(lv_color_hex(0xC2410C));
    _temp_caption->align(LV_ALIGN_TOP_LEFT, 12, 8);

    _temp_label = std::make_unique<Label>(*_temp_card);
    _temp_label->setText("--.- C");
    _temp_label->setTextFont(&lv_font_montserrat_20);
    _temp_label->setTextColor(lv_color_hex(0x7C2D12));
    _temp_label->align(LV_ALIGN_BOTTOM_LEFT, 12, -8);

    _humidity_card = std::make_unique<Container>(*_panel);
    _humidity_card->setSize(137, 62);
    _humidity_card->setBgColor(lv_color_hex(0xFFFFFF));
    _humidity_card->setBorderColor(lv_color_hex(0xBFDBFE));
    _humidity_card->setBorderWidth(2);
    _humidity_card->setRadius(8);
    _humidity_card->setPaddingAll(0);
    _humidity_card->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _humidity_card->align(LV_ALIGN_TOP_RIGHT, -16, 82);

    _humidity_caption = std::make_unique<Label>(*_humidity_card);
    _humidity_caption->setText("HUMIDITY");
    _humidity_caption->setTextFont(&lv_font_montserrat_14);
    _humidity_caption->setTextColor(lv_color_hex(0x1D4ED8));
    _humidity_caption->align(LV_ALIGN_TOP_LEFT, 12, 8);

    _humidity_label = std::make_unique<Label>(*_humidity_card);
    _humidity_label->setText("--.- %");
    _humidity_label->setTextFont(&lv_font_montserrat_20);
    _humidity_label->setTextColor(lv_color_hex(0x1E3A8A));
    _humidity_label->align(LV_ALIGN_BOTTOM_LEFT, 12, -8);

    _pressure_card = std::make_unique<Container>(*_panel);
    _pressure_card->setSize(137, 62);
    _pressure_card->setBgColor(lv_color_hex(0xFFFFFF));
    _pressure_card->setBorderColor(lv_color_hex(0xBBF7D0));
    _pressure_card->setBorderWidth(2);
    _pressure_card->setRadius(8);
    _pressure_card->setPaddingAll(0);
    _pressure_card->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _pressure_card->align(LV_ALIGN_TOP_LEFT, 16, 154);

    _pressure_caption = std::make_unique<Label>(*_pressure_card);
    _pressure_caption->setText("PRESSURE");
    _pressure_caption->setTextFont(&lv_font_montserrat_14);
    _pressure_caption->setTextColor(lv_color_hex(0x15803D));
    _pressure_caption->align(LV_ALIGN_TOP_LEFT, 12, 8);

    _pressure_label = std::make_unique<Label>(*_pressure_card);
    _pressure_label->setText("----.- hPa");
    _pressure_label->setTextFont(&lv_font_montserrat_16);
    _pressure_label->setTextColor(lv_color_hex(0x14532D));
    _pressure_label->align(LV_ALIGN_BOTTOM_LEFT, 12, -9);

    _altitude_card = std::make_unique<Container>(*_panel);
    _altitude_card->setSize(137, 62);
    _altitude_card->setBgColor(lv_color_hex(0xFFFFFF));
    _altitude_card->setBorderColor(lv_color_hex(0xDDD6FE));
    _altitude_card->setBorderWidth(2);
    _altitude_card->setRadius(8);
    _altitude_card->setPaddingAll(0);
    _altitude_card->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _altitude_card->align(LV_ALIGN_TOP_RIGHT, -16, 154);

    _altitude_caption = std::make_unique<Label>(*_altitude_card);
    _altitude_caption->setText("ALTITUDE");
    _altitude_caption->setTextFont(&lv_font_montserrat_14);
    _altitude_caption->setTextColor(lv_color_hex(0x6D28D9));
    _altitude_caption->align(LV_ALIGN_TOP_LEFT, 12, 8);

    _altitude_label = std::make_unique<Label>(*_altitude_card);
    _altitude_label->setText("--.- m");
    _altitude_label->setTextFont(&lv_font_montserrat_20);
    _altitude_label->setTextColor(lv_color_hex(0x4C1D95));
    _altitude_label->align(LV_ALIGN_BOTTOM_LEFT, 12, -8);

    _status_label = std::make_unique<Label>(*_panel);
    _status_label->setText("");
    _status_label->setTextFont(&lv_font_montserrat_14);
    _status_label->setTextColor(lv_color_hex(0x475569));
    _status_label->setWidth(280);
    _status_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _status_label->setLongMode(LV_LABEL_LONG_MODE_WRAP);
    _status_label->align(LV_ALIGN_TOP_MID, 0, 222);
}

void AppEnvIII::updateStatus(const std::string& status)
{
    if (_status_label) {
        _status_label->setText(status);
    }
}

void AppEnvIII::updateMeasurement(const Hal::EnvIIIData& data)
{
    _update_count++;
    _pulse_on = !_pulse_on;

    const uint32_t temp_color = temperatureColor(data.temperature_c);
    _temp_label->setText(fmt::format("{:.1f} C", data.temperature_c));
    _temp_label->setTextColor(lv_color_hex(temp_color));
    _humidity_label->setText(fmt::format("{:.1f} %", data.humidity_percent));
    _pressure_label->setText(fmt::format("{:.1f} hPa", data.pressure_hpa));
    _altitude_label->setText(fmt::format("{:.1f} m", data.altitude_m));
    _pulse_dot->setBgColor(lv_color_hex(_pulse_on ? temp_color : 0x3A8F7A));
    _update_label->setText(fmt::format("#{}", _update_count));
    updateStatus(fmt::format("Live data {}s", GetHAL().millis() / 1000));
}

#endif // CONFIG_HAL_ENVIII_ENABLED
