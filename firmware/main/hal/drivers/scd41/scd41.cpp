/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "scd41.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

SCD41::SCD41(i2c_master_bus_handle_t i2c_bus_handle, uint8_t addr) : _addr(addr)
{
    if (i2c_bus_handle == nullptr) {
        _init_error = ESP_ERR_INVALID_ARG;
        return;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = _addr,
        .scl_speed_hz    = 100000,
    };
    _init_error = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &_i2c_dev);
}

SCD41::~SCD41()
{
    if (_i2c_dev) {
        i2c_master_bus_rm_device(_i2c_dev);
    }
}

esp_err_t SCD41::begin()
{
    if (_init_error != ESP_OK) {
        return _init_error;
    }

    esp_err_t err = stopPeriodicMeasurement();
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(500));

    return startPeriodicMeasurement();
}

esp_err_t SCD41::startPeriodicMeasurement()
{
    return writeCommand(0x21B1);
}

esp_err_t SCD41::stopPeriodicMeasurement()
{
    return writeCommand(0x3F86);
}

esp_err_t SCD41::dataReady(bool& ready)
{
    ready = false;

    uint16_t status = 0;
    esp_err_t err = readWords(0xE4B8, &status, 1);
    if (err != ESP_OK) {
        return err;
    }

    ready = (status & 0x07FF) != 0;
    return ESP_OK;
}

esp_err_t SCD41::readMeasurement(SCD41Measurement& measurement)
{
    uint16_t words[3] = {};
    esp_err_t err = readWords(0xEC05, words, 3);
    if (err != ESP_OK) {
        return err;
    }

    measurement.co2_ppm = words[0];
    measurement.temperature_c = -45.0f + 175.0f * static_cast<float>(words[1]) / 65535.0f;
    measurement.humidity_percent = 100.0f * static_cast<float>(words[2]) / 65535.0f;
    return ESP_OK;
}

esp_err_t SCD41::writeCommand(uint16_t command)
{
    if (_i2c_dev == nullptr) {
        return _init_error == ESP_OK ? ESP_ERR_INVALID_STATE : _init_error;
    }

    uint8_t data[2] = {
        static_cast<uint8_t>(command >> 8),
        static_cast<uint8_t>(command & 0xFF),
    };
    return i2c_master_transmit(_i2c_dev, data, sizeof(data), 1000);
}

esp_err_t SCD41::readWords(uint16_t command, uint16_t* words, size_t word_count)
{
    if (_i2c_dev == nullptr || words == nullptr || word_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t command_data[2] = {
        static_cast<uint8_t>(command >> 8),
        static_cast<uint8_t>(command & 0xFF),
    };
    uint8_t read_data[9] = {};
    const size_t read_len = word_count * 3;
    if (read_len > sizeof(read_data)) {
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t err = i2c_master_transmit(_i2c_dev, command_data, sizeof(command_data), 1000);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(2));

    err = i2c_master_receive(_i2c_dev, read_data, read_len, 1000);
    if (err != ESP_OK) {
        return err;
    }

    for (size_t i = 0; i < word_count; ++i) {
        const uint8_t* word_data = &read_data[i * 3];
        if (!validateWordCrc(word_data)) {
            return ESP_ERR_INVALID_CRC;
        }
        words[i] = (static_cast<uint16_t>(word_data[0]) << 8) | word_data[1];
    }

    return ESP_OK;
}

uint8_t SCD41::crc8(const uint8_t* data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x80) {
                crc = static_cast<uint8_t>((crc << 1) ^ 0x31);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

bool SCD41::validateWordCrc(const uint8_t* data)
{
    return crc8(data, 2) == data[2];
}
