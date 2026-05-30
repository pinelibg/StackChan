/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include "board/hal_bridge.h"
#include "drivers/scd41/scd41.h"
#include <mooncake_log.h>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const std::string_view _tag = "HAL-SCD41";

static std::unique_ptr<SCD41> _scd41;

static void _scd41_task(void* param)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(500));

        if (!_scd41) {
            continue;
        }

        bool ready = false;
        if (_scd41->dataReady(ready) != ESP_OK || !ready) {
            continue;
        }

        SCD41Measurement measurement;
        if (_scd41->readMeasurement(measurement) != ESP_OK) {
            continue;
        }

        GetHAL().onScd41Measurement.emit(Hal::Scd41Data{
            .co2_ppm          = measurement.co2_ppm,
            .temperature_c    = measurement.temperature_c,
            .humidity_percent = measurement.humidity_percent,
        });
    }
}

void Hal::scd41_init()
{
    mclog::tagInfo(_tag, "init");

    auto i2c_bus = hal_bridge::board_get_port_a_i2c_bus();
    if (!i2c_bus) {
        mclog::tagError(_tag, "port A I2C bus not available");
        return;
    }

    _scd41 = std::make_unique<SCD41>(i2c_bus);
    if (_scd41->begin() != ESP_OK) {
        _scd41.reset();
        mclog::tagError(_tag, "SCD41 init failed");
        return;
    }

    mclog::tagInfo(_tag, "SCD41 init ok");

    xTaskCreatePinnedToCoreWithCaps(_scd41_task, "scd41", 4096, NULL, 2, NULL, 1, MALLOC_CAP_SPIRAM);
}
