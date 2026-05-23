/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>
#include "driver/i2c_master.h"
#include "esp_err.h"

struct SCD41Measurement {
    uint16_t co2_ppm = 0;
    float temperature_c = 0.0f;
    float humidity_percent = 0.0f;
};

class SCD41 {
public:
    static constexpr uint8_t DEFAULT_ADDRESS = 0x62;

    SCD41(i2c_master_bus_handle_t i2c_bus_handle, uint8_t addr = DEFAULT_ADDRESS);
    ~SCD41();

    esp_err_t begin();
    esp_err_t startPeriodicMeasurement();
    esp_err_t stopPeriodicMeasurement();
    esp_err_t dataReady(bool& ready);
    esp_err_t readMeasurement(SCD41Measurement& measurement);

private:
    i2c_master_dev_handle_t _i2c_dev = nullptr;
    esp_err_t _init_error = ESP_OK;
    uint8_t _addr = DEFAULT_ADDRESS;

    esp_err_t writeCommand(uint16_t command);
    esp_err_t readWords(uint16_t command, uint16_t* words, size_t word_count);
    static uint8_t crc8(const uint8_t* data, size_t len);
    static bool validateWordCrc(const uint8_t* data);
};
