/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "enviii.h"
#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mooncake_log.h>

namespace {
static const char* _tag = "ENVIII";
constexpr uint8_t QMP6988_CHIP_ID = 0x5C;
constexpr uint8_t QMP6988_CHIP_ID_REG = 0xD1;
constexpr uint8_t QMP6988_RESET_REG = 0xE0;
constexpr uint8_t QMP6988_CONFIG_REG = 0xF1;
constexpr uint8_t QMP6988_CTRLMEAS_REG = 0xF4;
constexpr uint8_t QMP6988_PRESSURE_MSB_REG = 0xF7;
constexpr uint8_t QMP6988_CALIBRATION_DATA_START = 0xA0;
constexpr size_t QMP6988_CALIBRATION_DATA_LENGTH = 25;
constexpr int32_t QMP6988_SUBTRACTOR = 8388608;
constexpr int kI2cTimeoutMs = 1000;
}  // namespace

EnvIII::EnvIII(i2c_master_bus_handle_t i2c_bus_handle, uint8_t sht30_addr, uint8_t qmp6988_addr)
    : _sht30_addr(sht30_addr), _qmp6988_addr(qmp6988_addr)
{
    if (i2c_bus_handle == nullptr) {
        _sht30_init_error = ESP_ERR_INVALID_ARG;
        _qmp6988_init_error = ESP_ERR_INVALID_ARG;
        return;
    }

    i2c_device_config_t sht30_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = _sht30_addr,
        .scl_speed_hz = 100000,
    };
    _sht30_init_error = i2c_master_bus_add_device(i2c_bus_handle, &sht30_cfg, &_sht30_dev);

    i2c_device_config_t qmp6988_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = _qmp6988_addr,
        .scl_speed_hz = 100000,
    };
    _qmp6988_init_error = i2c_master_bus_add_device(i2c_bus_handle, &qmp6988_cfg, &_qmp6988_dev);
}

EnvIII::~EnvIII()
{
    if (_sht30_dev) {
        i2c_master_bus_rm_device(_sht30_dev);
    }
    if (_qmp6988_dev) {
        i2c_master_bus_rm_device(_qmp6988_dev);
    }
}

esp_err_t EnvIII::begin()
{
    if (_sht30_init_error != ESP_OK) {
        return _sht30_init_error;
    }
    if (_qmp6988_init_error != ESP_OK) {
        return _qmp6988_init_error;
    }

    uint8_t chip_id = 0;
    esp_err_t err = qmpReadRegs(QMP6988_CHIP_ID_REG, &chip_id, 1);
    if (err != ESP_OK) {
        return err;
    }
    mclog::tagInfo(_tag, "QMP6988 chip id: 0x{:02X}", chip_id);
    if (chip_id != QMP6988_CHIP_ID) {
        return ESP_ERR_NOT_FOUND;
    }

    err = qmpWriteReg(QMP6988_RESET_REG, 0xE6);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    err = qmpWriteReg(QMP6988_RESET_REG, 0x00);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(20));

    err = readQmpCalibration();
    if (err != ESP_OK) {
        return err;
    }
    err = qmpWriteReg(QMP6988_CONFIG_REG, 0x02);
    if (err != ESP_OK) {
        return err;
    }
    err = qmpUpdateReg(QMP6988_CTRLMEAS_REG, 0x1C, 0x04 << 2);
    if (err != ESP_OK) {
        return err;
    }
    err = qmpUpdateReg(QMP6988_CTRLMEAS_REG, 0xE0, 0x01 << 5);
    if (err != ESP_OK) {
        return err;
    }
    return qmpUpdateReg(QMP6988_CTRLMEAS_REG, 0x03, 0x03);
}

esp_err_t EnvIII::readMeasurement(EnvIIIMeasurement& measurement)
{
    esp_err_t err = readSht30(measurement.temperature_c, measurement.humidity_percent);
    if (err != ESP_OK) {
        return err;
    }

    err = readQmp6988(measurement.pressure_pa, measurement.qmp_temperature_c);
    if (err != ESP_OK) {
        return err;
    }

    measurement.pressure_hpa = measurement.pressure_pa / 100.0f;
    measurement.altitude_m = calcAltitude(measurement.pressure_pa, measurement.temperature_c);
    return ESP_OK;
}

esp_err_t EnvIII::readSht30(float& temperature_c, float& humidity_percent)
{
    if (_sht30_dev == nullptr) {
        return _sht30_init_error == ESP_OK ? ESP_ERR_INVALID_STATE : _sht30_init_error;
    }

    const uint8_t command[2] = {0x2C, 0x06};
    uint8_t data[6] = {};
    esp_err_t err = i2c_master_transmit(_sht30_dev, command, sizeof(command), kI2cTimeoutMs);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    err = i2c_master_receive(_sht30_dev, data, sizeof(data), kI2cTimeoutMs);
    if (err != ESP_OK) {
        return err;
    }
    if (!validateSht30Word(data) || !validateSht30Word(data + 3)) {
        return ESP_ERR_INVALID_CRC;
    }

    const uint16_t raw_temp = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    const uint16_t raw_humidity = (static_cast<uint16_t>(data[3]) << 8) | data[4];
    temperature_c = -45.0f + 175.0f * static_cast<float>(raw_temp) / 65535.0f;
    humidity_percent = 100.0f * static_cast<float>(raw_humidity) / 65535.0f;
    return ESP_OK;
}

esp_err_t EnvIII::readQmp6988(float& pressure_pa, float& temperature_c)
{
    uint8_t data[6] = {};
    esp_err_t err = qmpReadRegs(QMP6988_PRESSURE_MSB_REG, data, sizeof(data));
    if (err != ESP_OK) {
        return err;
    }

    const uint32_t p_read = (static_cast<uint32_t>(data[0]) << 16) | (static_cast<uint32_t>(data[1]) << 8) | data[2];
    const uint32_t t_read = (static_cast<uint32_t>(data[3]) << 16) | (static_cast<uint32_t>(data[4]) << 8) | data[5];
    const int32_t p_raw = static_cast<int32_t>(p_read) - QMP6988_SUBTRACTOR;
    const int32_t t_raw = static_cast<int32_t>(t_read) - QMP6988_SUBTRACTOR;

    const int16_t t_int = qmpConvTemperature(t_raw);
    const int32_t p_int = qmpConvPressure(p_raw, t_int);
    temperature_c = static_cast<float>(t_int) / 256.0f;
    pressure_pa = static_cast<float>(p_int) / 16.0f;

    if (pressure_pa < 30000.0f || pressure_pa > 120000.0f) {
        mclog::tagError(_tag,
                        "QMP6988 invalid pressure: raw={:02X} {:02X} {:02X} {:02X} {:02X} {:02X}, p_read={}, t_read={}, p_raw={}, t_raw={}, t_int={}, p_int={}, pressure={:.2f} Pa",
                        data[0], data[1], data[2], data[3], data[4], data[5], p_read, t_read, p_raw, t_raw, t_int,
                        p_int, pressure_pa);
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}

esp_err_t EnvIII::readQmpCalibration()
{
    uint8_t data[QMP6988_CALIBRATION_DATA_LENGTH] = {};
    for (size_t i = 0; i < QMP6988_CALIBRATION_DATA_LENGTH; ++i) {
        const uint8_t reg = QMP6988_CALIBRATION_DATA_START + i;
        esp_err_t err = qmpReadRegs(reg, &data[i], 1);
        if (err != ESP_OK) {
            mclog::tagError(_tag, "QMP6988 calibration read failed at 0x{:02X}: {}", reg, esp_err_to_name(err));
            return err;
        }
    }

    _qmp_cali.a0 = signExtend20((static_cast<uint32_t>(data[18]) << 12) | (static_cast<uint32_t>(data[19]) << 4) |
                                (data[24] & 0x0F));
    _qmp_cali.a1 = static_cast<int16_t>((static_cast<uint16_t>(data[20]) << 8) | data[21]);
    _qmp_cali.a2 = static_cast<int16_t>((static_cast<uint16_t>(data[22]) << 8) | data[23]);
    _qmp_cali.b00 = signExtend20((static_cast<uint32_t>(data[0]) << 12) | (static_cast<uint32_t>(data[1]) << 4) |
                                 ((data[24] & 0xF0) >> 4));
    _qmp_cali.bt1 = static_cast<int16_t>((static_cast<uint16_t>(data[2]) << 8) | data[3]);
    _qmp_cali.bt2 = static_cast<int16_t>((static_cast<uint16_t>(data[4]) << 8) | data[5]);
    _qmp_cali.bp1 = static_cast<int16_t>((static_cast<uint16_t>(data[6]) << 8) | data[7]);
    _qmp_cali.b11 = static_cast<int16_t>((static_cast<uint16_t>(data[8]) << 8) | data[9]);
    _qmp_cali.bp2 = static_cast<int16_t>((static_cast<uint16_t>(data[10]) << 8) | data[11]);
    _qmp_cali.b12 = static_cast<int16_t>((static_cast<uint16_t>(data[12]) << 8) | data[13]);
    _qmp_cali.b21 = static_cast<int16_t>((static_cast<uint16_t>(data[14]) << 8) | data[15]);
    _qmp_cali.bp3 = static_cast<int16_t>((static_cast<uint16_t>(data[16]) << 8) | data[17]);

    _qmp_ik.a0 = _qmp_cali.a0;
    _qmp_ik.b00 = _qmp_cali.b00;
    _qmp_ik.a1 = 3608L * static_cast<int32_t>(_qmp_cali.a1) - 1731677965L;
    _qmp_ik.a2 = 16889L * static_cast<int32_t>(_qmp_cali.a2) - 87619360L;
    _qmp_ik.bt1 = 2982L * static_cast<int64_t>(_qmp_cali.bt1) + 107370906L;
    _qmp_ik.bt2 = 329854L * static_cast<int64_t>(_qmp_cali.bt2) + 108083093L;
    _qmp_ik.bp1 = 19923L * static_cast<int64_t>(_qmp_cali.bp1) + 1133836764L;
    _qmp_ik.b11 = 2406L * static_cast<int64_t>(_qmp_cali.b11) + 118215883L;
    _qmp_ik.bp2 = 3079L * static_cast<int64_t>(_qmp_cali.bp2) - 181579595L;
    _qmp_ik.b12 = 6846L * static_cast<int64_t>(_qmp_cali.b12) + 85590281L;
    _qmp_ik.b21 = 13836L * static_cast<int64_t>(_qmp_cali.b21) + 79333336L;
    _qmp_ik.bp3 = 2915L * static_cast<int64_t>(_qmp_cali.bp3) + 157155561L;

    return ESP_OK;
}

esp_err_t EnvIII::qmpWriteReg(uint8_t reg, uint8_t value)
{
    if (_qmp6988_dev == nullptr) {
        return _qmp6988_init_error == ESP_OK ? ESP_ERR_INVALID_STATE : _qmp6988_init_error;
    }

    const uint8_t data[2] = {reg, value};
    return i2c_master_transmit(_qmp6988_dev, data, sizeof(data), kI2cTimeoutMs);
}

esp_err_t EnvIII::qmpReadRegs(uint8_t reg, uint8_t* data, size_t len)
{
    if (_qmp6988_dev == nullptr || data == nullptr || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_transmit_receive(_qmp6988_dev, &reg, 1, data, len, kI2cTimeoutMs);
}

esp_err_t EnvIII::qmpUpdateReg(uint8_t reg, uint8_t clear_mask, uint8_t value)
{
    uint8_t data = 0;
    esp_err_t err = qmpReadRegs(reg, &data, 1);
    if (err != ESP_OK) {
        return err;
    }

    data = static_cast<uint8_t>((data & ~clear_mask) | value);
    err = qmpWriteReg(reg, data);
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return err;
}

int16_t EnvIII::qmpConvTemperature(int32_t dt) const
{
    int64_t wk1 = static_cast<int64_t>(_qmp_ik.a1) * static_cast<int64_t>(dt);
    int64_t wk2 = (static_cast<int64_t>(_qmp_ik.a2) * static_cast<int64_t>(dt)) >> 14;
    wk2 = (wk2 * static_cast<int64_t>(dt)) >> 10;
    wk2 = ((wk1 + wk2) / 32767) >> 19;
    return static_cast<int16_t>((_qmp_ik.a0 + wk2) >> 4);
}

int32_t EnvIII::qmpConvPressure(int32_t dp, int16_t tx) const
{
    int64_t wk1 = static_cast<int64_t>(_qmp_ik.bt1) * static_cast<int64_t>(tx);
    int64_t wk2 = (static_cast<int64_t>(_qmp_ik.bp1) * static_cast<int64_t>(dp)) >> 5;
    wk1 += wk2;
    wk2 = (static_cast<int64_t>(_qmp_ik.bt2) * static_cast<int64_t>(tx)) >> 1;
    wk2 = (wk2 * static_cast<int64_t>(tx)) >> 8;
    int64_t wk3 = wk2;
    wk2 = (static_cast<int64_t>(_qmp_ik.b11) * static_cast<int64_t>(tx)) >> 4;
    wk2 = (wk2 * static_cast<int64_t>(dp)) >> 1;
    wk3 += wk2;
    wk2 = (static_cast<int64_t>(_qmp_ik.bp2) * static_cast<int64_t>(dp)) >> 13;
    wk2 = (wk2 * static_cast<int64_t>(dp)) >> 1;
    wk3 += wk2;
    wk1 += wk3 >> 14;
    wk2 = static_cast<int64_t>(_qmp_ik.b12) * static_cast<int64_t>(tx);
    wk2 = (wk2 * static_cast<int64_t>(tx)) >> 22;
    wk2 = (wk2 * static_cast<int64_t>(dp)) >> 1;
    wk3 = wk2;
    wk2 = (static_cast<int64_t>(_qmp_ik.b21) * static_cast<int64_t>(tx)) >> 6;
    wk2 = (wk2 * static_cast<int64_t>(dp)) >> 23;
    wk2 = (wk2 * static_cast<int64_t>(dp)) >> 1;
    wk3 += wk2;
    wk2 = (static_cast<int64_t>(_qmp_ik.bp3) * static_cast<int64_t>(dp)) >> 12;
    wk2 = (wk2 * static_cast<int64_t>(dp)) >> 23;
    wk2 = wk2 * static_cast<int64_t>(dp);
    wk3 += wk2;
    wk1 += wk3 >> 15;
    wk1 /= 32767L;
    wk1 >>= 11;
    wk1 += _qmp_ik.b00;
    return static_cast<int32_t>(wk1);
}

uint8_t EnvIII::crc8(const uint8_t* data, size_t len)
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

bool EnvIII::validateSht30Word(const uint8_t* data)
{
    return crc8(data, 2) == data[2];
}

int32_t EnvIII::signExtend20(uint32_t value)
{
    value &= 0xFFFFF;
    if (value & 0x80000) {
        value |= 0xFFF00000;
    }
    return static_cast<int32_t>(value);
}

float EnvIII::calcAltitude(float pressure_pa, float temperature_c)
{
    if (pressure_pa <= 0.0f) {
        return 0.0f;
    }
    return (std::pow(101325.0f / pressure_pa, 1.0f / 5.257f) - 1.0f) * (temperature_c + 273.15f) / 0.0065f;
}
