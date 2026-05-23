/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <hal/drivers/scd41/scd41.h>
#include <mooncake.h>
#include <smooth_lvgl.hpp>
#include <memory>
#include <string>

class AppScd41 : public mooncake::AppAbility {
public:
    AppScd41();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::unique_ptr<SCD41> _sensor;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _panel;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _title;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _sensor_name_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _status_pill;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _pulse_dot;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _co2_card;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _co2_caption;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _co2_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _co2_unit_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _co2_meter_bg;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _co2_meter_fill;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _temp_card;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _temp_caption;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _temp_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _humidity_card;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _humidity_caption;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _humidity_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _status_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _update_label;

    uint32_t _last_read_tick = 0;
    uint32_t _update_count = 0;
    bool _sensor_ready = false;
    bool _pulse_on = false;

    void createView();
    void updateStatus(const std::string& status);
    void updateMeasurement(const SCD41Measurement& measurement);
};
