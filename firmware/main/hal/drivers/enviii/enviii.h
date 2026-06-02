/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include "driver/i2c_master.h"
#include "esp_err.h"

struct EnvIIIMeasurement {
    float temperature_c = 0.0f;
    float humidity_percent = 0.0f;
    float pressure_pa = 0.0f;
    float pressure_hpa = 0.0f;
    float qmp_temperature_c = 0.0f;
    float altitude_m = 0.0f;
};

class EnvIII {
public:
    static constexpr uint8_t DEFAULT_SHT30_ADDRESS = 0x44;
    static constexpr uint8_t DEFAULT_QMP6988_ADDRESS = 0x70;

    EnvIII(i2c_master_bus_handle_t i2c_bus_handle, uint8_t sht30_addr = DEFAULT_SHT30_ADDRESS,
           uint8_t qmp6988_addr = DEFAULT_QMP6988_ADDRESS);
    ~EnvIII();

    esp_err_t begin();
    esp_err_t readMeasurement(EnvIIIMeasurement& measurement);

private:
    struct QmpCalibration {
        int32_t a0 = 0;
        int16_t a1 = 0;
        int16_t a2 = 0;
        int32_t b00 = 0;
        int16_t bt1 = 0;
        int16_t bt2 = 0;
        int16_t bp1 = 0;
        int16_t b11 = 0;
        int16_t bp2 = 0;
        int16_t b12 = 0;
        int16_t b21 = 0;
        int16_t bp3 = 0;
    };

    struct QmpIntCalibration {
        int32_t a0 = 0;
        int32_t b00 = 0;
        int32_t a1 = 0;
        int32_t a2 = 0;
        int64_t bt1 = 0;
        int64_t bt2 = 0;
        int64_t bp1 = 0;
        int64_t b11 = 0;
        int64_t bp2 = 0;
        int64_t b12 = 0;
        int64_t b21 = 0;
        int64_t bp3 = 0;
    };

    i2c_master_dev_handle_t _sht30_dev = nullptr;
    i2c_master_dev_handle_t _qmp6988_dev = nullptr;
    esp_err_t _sht30_init_error = ESP_OK;
    esp_err_t _qmp6988_init_error = ESP_OK;
    uint8_t _sht30_addr = DEFAULT_SHT30_ADDRESS;
    uint8_t _qmp6988_addr = DEFAULT_QMP6988_ADDRESS;
    QmpCalibration _qmp_cali;
    QmpIntCalibration _qmp_ik;

    esp_err_t readSht30(float& temperature_c, float& humidity_percent);
    esp_err_t readQmp6988(float& pressure_pa, float& temperature_c);

    esp_err_t readQmpCalibration();
    esp_err_t qmpWriteReg(uint8_t reg, uint8_t value);
    esp_err_t qmpReadRegs(uint8_t reg, uint8_t* data, size_t len);
    esp_err_t qmpUpdateReg(uint8_t reg, uint8_t clear_mask, uint8_t value);
    int16_t qmpConvTemperature(int32_t dt) const;
    int32_t qmpConvPressure(int32_t dp, int16_t tx) const;

    static uint8_t crc8(const uint8_t* data, size_t len);
    static bool validateSht30Word(const uint8_t* data);
    static int32_t signExtend20(uint32_t value);
    static float calcAltitude(float pressure_pa, float temperature_c);
};
