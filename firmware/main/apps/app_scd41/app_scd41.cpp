/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_scd41.h"
#include <apps/common/common.h>
#include <fmt/format.h>
#include <hal/hal.h>
#include <mooncake_log.h>

using namespace smooth_ui_toolkit::lvgl_cpp;

namespace {
uint32_t co2Color(uint16_t ppm)
{
    if (ppm >= 1200) {
        return 0xEF4444;
    }
    if (ppm >= 800) {
        return 0xF59E0B;
    }
    return 0x22C55E;
}

int32_t co2MeterWidth(uint16_t ppm)
{
    if (ppm <= 400) {
        return 12;
    }
    if (ppm >= 2000) {
        return 224;
    }
    return 12 + ((ppm - 400) * 212 / 1600);
}
}  // namespace

AppScd41::AppScd41()
{
    setAppInfo().name           = "CO2";
    static uint32_t theme_color = 0x2FB8A0;
    setAppInfo().userData       = (void*)&theme_color;
}

void AppScd41::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppScd41::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    {
        LvglLockGuard lock;
        createView();
        updateStatus("Initializing PORT.A sensor...");
        view::create_home_indicator([&]() { close(); }, 0x74D8C8, 0x083B35);
        view::create_status_bar(0x74D8C8, 0x083B35);
    }

    _scd41_conn_id = GetHAL().onScd41Measurement.connect([this](const Hal::Scd41Data& data) {
        LvglLockGuard lock;
        updateMeasurement(data);
    });

    LvglLockGuard lock;
    updateStatus("Waiting for first measurement...");
}

void AppScd41::onRunning()
{
    LvglLockGuard lock;
    view::update_home_indicator();
    view::update_status_bar();
}

void AppScd41::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    GetHAL().onScd41Measurement.disconnect(_scd41_conn_id);
    _scd41_conn_id = -1;

    LvglLockGuard lock;
    _title.reset();
    _sensor_name_label.reset();
    _pulse_dot.reset();
    _update_label.reset();
    _status_pill.reset();
    _co2_caption.reset();
    _co2_label.reset();
    _co2_unit_label.reset();
    _co2_meter_fill.reset();
    _co2_meter_bg.reset();
    _co2_card.reset();
    _temp_caption.reset();
    _temp_label.reset();
    _temp_card.reset();
    _humidity_caption.reset();
    _humidity_label.reset();
    _humidity_card.reset();
    _status_label.reset();
    _panel.reset();
    _update_count = 0;
    _pulse_on     = false;

    view::destroy_home_indicator();
    view::destroy_status_bar();
}

void AppScd41::createView()
{
    _panel = std::make_unique<Container>(lv_screen_active());
    _panel->setSize(320, 240);
    _panel->setAlign(LV_ALIGN_CENTER);
    _panel->setBgColor(lv_color_hex(0xF3F8FF));
    _panel->setBorderWidth(0);
    _panel->setRadius(0);
    _panel->setPaddingAll(0);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _title = std::make_unique<Label>(*_panel);
    _title->setText("M5Stack SCD41");
    _title->setTextFont(&lv_font_montserrat_20);
    _title->setTextColor(lv_color_hex(0x102A43));
    _title->align(LV_ALIGN_TOP_LEFT, 16, 30);

    _sensor_name_label = std::make_unique<Label>(*_panel);
    _sensor_name_label->setText("Temp / RH / CO2 Sensor");
    _sensor_name_label->setTextFont(&lv_font_montserrat_14);
    _sensor_name_label->setTextColor(lv_color_hex(0x52616B));
    _sensor_name_label->align(LV_ALIGN_TOP_LEFT, 17, 54);

    _status_pill = std::make_unique<Container>(*_panel);
    _status_pill->setSize(82, 26);
    _status_pill->setBgColor(lv_color_hex(0xE0F2FE));
    _status_pill->setBorderColor(lv_color_hex(0x7DD3FC));
    _status_pill->setBorderWidth(2);
    _status_pill->setRadius(13);
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
    _update_label->setTextColor(lv_color_hex(0x075985));
    _update_label->setWidth(52);
    _update_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _update_label->align(LV_ALIGN_RIGHT_MID, -4, 0);

    _co2_card = std::make_unique<Container>(*_panel);
    _co2_card->setSize(288, 94);
    _co2_card->setBgColor(lv_color_hex(0xFFFFFF));
    _co2_card->setBorderColor(lv_color_hex(0xBAE6FD));
    _co2_card->setBorderWidth(2);
    _co2_card->setRadius(18);
    _co2_card->setPaddingAll(0);
    _co2_card->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _co2_card->align(LV_ALIGN_TOP_MID, 0, 76);

    _co2_caption = std::make_unique<Label>(*_co2_card);
    _co2_caption->setText("CO2");
    _co2_caption->setTextFont(&lv_font_montserrat_16);
    _co2_caption->setTextColor(lv_color_hex(0x0F766E));
    _co2_caption->align(LV_ALIGN_TOP_LEFT, 18, 12);

    _co2_label = std::make_unique<Label>(*_co2_card);
    _co2_label->setText("--");
    _co2_label->setTextFont(&lv_font_montserrat_24);
    _co2_label->setTextColor(lv_color_hex(0x0F172A));
    _co2_label->align(LV_ALIGN_LEFT_MID, 18, -8);

    _co2_unit_label = std::make_unique<Label>(*_co2_card);
    _co2_unit_label->setText("ppm");
    _co2_unit_label->setTextFont(&lv_font_montserrat_16);
    _co2_unit_label->setTextColor(lv_color_hex(0x64748B));
    _co2_unit_label->align(LV_ALIGN_LEFT_MID, 112, -3);

    _co2_meter_bg = std::make_unique<Container>(*_co2_card);
    _co2_meter_bg->setSize(224, 12);
    _co2_meter_bg->setBgColor(lv_color_hex(0xE2E8F0));
    _co2_meter_bg->setBorderWidth(0);
    _co2_meter_bg->setRadius(6);
    _co2_meter_bg->setPaddingAll(0);
    _co2_meter_bg->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _co2_meter_bg->align(LV_ALIGN_BOTTOM_LEFT, 18, -14);

    _co2_meter_fill = std::make_unique<Container>(*_co2_meter_bg);
    _co2_meter_fill->setSize(12, 12);
    _co2_meter_fill->setBgColor(lv_color_hex(0x94A3B8));
    _co2_meter_fill->setBorderWidth(0);
    _co2_meter_fill->setRadius(6);
    _co2_meter_fill->setPaddingAll(0);
    _co2_meter_fill->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _co2_meter_fill->align(LV_ALIGN_LEFT_MID, 0, 0);

    _temp_card = std::make_unique<Container>(*_panel);
    _temp_card->setSize(137, 44);
    _temp_card->setBgColor(lv_color_hex(0xFFF7ED));
    _temp_card->setBorderColor(lv_color_hex(0xFDBA74));
    _temp_card->setBorderWidth(2);
    _temp_card->setRadius(14);
    _temp_card->setPaddingAll(0);
    _temp_card->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _temp_card->align(LV_ALIGN_TOP_LEFT, 16, 178);

    _temp_caption = std::make_unique<Label>(*_temp_card);
    _temp_caption->setText("TEMP");
    _temp_caption->setTextFont(&lv_font_montserrat_14);
    _temp_caption->setTextColor(lv_color_hex(0xC2410C));
    _temp_caption->align(LV_ALIGN_TOP_LEFT, 12, 5);

    _temp_label = std::make_unique<Label>(*_temp_card);
    _temp_label->setText("--.- C");
    _temp_label->setTextFont(&lv_font_montserrat_20);
    _temp_label->setTextColor(lv_color_hex(0x7C2D12));
    _temp_label->align(LV_ALIGN_BOTTOM_LEFT, 12, -5);

    _humidity_card = std::make_unique<Container>(*_panel);
    _humidity_card->setSize(137, 44);
    _humidity_card->setBgColor(lv_color_hex(0xEEF2FF));
    _humidity_card->setBorderColor(lv_color_hex(0xA5B4FC));
    _humidity_card->setBorderWidth(2);
    _humidity_card->setRadius(14);
    _humidity_card->setPaddingAll(0);
    _humidity_card->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _humidity_card->align(LV_ALIGN_TOP_RIGHT, -16, 178);

    _humidity_caption = std::make_unique<Label>(*_humidity_card);
    _humidity_caption->setText("HUMIDITY");
    _humidity_caption->setTextFont(&lv_font_montserrat_14);
    _humidity_caption->setTextColor(lv_color_hex(0x4338CA));
    _humidity_caption->align(LV_ALIGN_TOP_LEFT, 12, 5);

    _humidity_label = std::make_unique<Label>(*_humidity_card);
    _humidity_label->setText("--.- %");
    _humidity_label->setTextFont(&lv_font_montserrat_20);
    _humidity_label->setTextColor(lv_color_hex(0x312E81));
    _humidity_label->align(LV_ALIGN_BOTTOM_LEFT, 12, -5);

    _status_label = std::make_unique<Label>(*_panel);
    _status_label->setText("");
    _status_label->setTextFont(&lv_font_montserrat_14);
    _status_label->setTextColor(lv_color_hex(0x475569));
    _status_label->setWidth(280);
    _status_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _status_label->setLongMode(LV_LABEL_LONG_MODE_WRAP);
    _status_label->align(LV_ALIGN_TOP_MID, 0, 222);
}

void AppScd41::updateStatus(const std::string& status)
{
    if (_status_label) {
        _status_label->setText(status);
    }
}

void AppScd41::updateMeasurement(const Hal::Scd41Data& data)
{
    _update_count++;
    _pulse_on = !_pulse_on;

    const uint32_t color = co2Color(data.co2_ppm);
    _co2_label->setText(fmt::format("{}", data.co2_ppm));
    _co2_label->setTextColor(lv_color_hex(color));
    _co2_meter_fill->setWidth(co2MeterWidth(data.co2_ppm));
    _co2_meter_fill->setBgColor(lv_color_hex(color));
    _pulse_dot->setBgColor(lv_color_hex(_pulse_on ? color : 0x38BDF8));
    _update_label->setText(fmt::format("#{}", _update_count));
    _temp_label->setText(fmt::format("{:.1f} C", data.temperature_c));
    _humidity_label->setText(fmt::format("{:.1f} %", data.humidity_percent));
    updateStatus(fmt::format("Live data {}s", GetHAL().millis() / 1000));
}
