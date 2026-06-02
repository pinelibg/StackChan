/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "sdkconfig.h"

#if CONFIG_HAL_ENVIII_ENABLED

#include <hal/hal.h>
#include <memory>
#include <mooncake.h>
#include <smooth_lvgl.hpp>
#include <string>

class AppEnvIII : public mooncake::AppAbility {
public:
    AppEnvIII();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _panel;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _title;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _sensor_name_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _status_pill;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _pulse_dot;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _update_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _temp_card;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _temp_caption;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _temp_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _humidity_card;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _humidity_caption;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _humidity_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _pressure_card;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _pressure_caption;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _pressure_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _altitude_card;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _altitude_caption;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _altitude_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _status_label;

    int _enviii_conn_id = -1;
    uint32_t _update_count = 0;
    bool _pulse_on = false;

    void createView();
    void updateStatus(const std::string& status);
    void updateMeasurement(const Hal::EnvIIIData& data);
};

#endif // CONFIG_HAL_ENVIII_ENABLED
