#include "../../vl53l0x_def.h"
#include "../../vl53l0x_i2c_platform.h"
#include "esphome/components/i2c/i2c.h"

// #define I2C_DEBUG

int VL53L0X_i2c_init(esphome::i2c::I2CBus *i2c) {
  // I2C initialization is handled by ESPHome
  return VL53L0X_ERROR_NONE;
}

int VL53L0X_write_multi(uint8_t deviceAddress, uint8_t index, uint8_t *pdata,
                        uint32_t count, esphome::i2c::I2CBus *i2c) {
  // Build buffer with register index followed by data
  uint8_t buffer[65]; // Max 64 bytes data + 1 byte index
  if (count > 64) {
    return VL53L0X_ERROR_INVALID_PARAMS;
  }

  buffer[0] = index;
  for (uint32_t i = 0; i < count; i++) {
    buffer[i + 1] = pdata[i];
  }

  esphome::i2c::ErrorCode err =
      i2c->write(deviceAddress, buffer, count + 1, true);

  if (err != esphome::i2c::ERROR_OK) {
    return VL53L0X_ERROR_CONTROL_INTERFACE;
  }
  return VL53L0X_ERROR_NONE;
}

int VL53L0X_read_multi(uint8_t deviceAddress, uint8_t index, uint8_t *pdata,
                       uint32_t count, esphome::i2c::I2CBus *i2c) {
  // First write the register index
  esphome::i2c::ErrorCode err = i2c->write(deviceAddress, &index, 1, false);
  if (err != esphome::i2c::ERROR_OK) {
    return VL53L0X_ERROR_CONTROL_INTERFACE;
  }

  // Then read the data
  err = i2c->read(deviceAddress, pdata, count);
  if (err != esphome::i2c::ERROR_OK) {
    return VL53L0X_ERROR_CONTROL_INTERFACE;
  }

  return VL53L0X_ERROR_NONE;
}

int VL53L0X_write_byte(uint8_t deviceAddress, uint8_t index, uint8_t data,
                       esphome::i2c::I2CBus *i2c) {
  return VL53L0X_write_multi(deviceAddress, index, &data, 1, i2c);
}

int VL53L0X_write_word(uint8_t deviceAddress, uint8_t index, uint16_t data,
                       esphome::i2c::I2CBus *i2c) {
  uint8_t buff[2];
  buff[1] = data & 0xFF;
  buff[0] = data >> 8;
  return VL53L0X_write_multi(deviceAddress, index, buff, 2, i2c);
}

int VL53L0X_write_dword(uint8_t deviceAddress, uint8_t index, uint32_t data,
                        esphome::i2c::I2CBus *i2c) {
  uint8_t buff[4];

  buff[3] = data & 0xFF;
  buff[2] = data >> 8;
  buff[1] = data >> 16;
  buff[0] = data >> 24;

  return VL53L0X_write_multi(deviceAddress, index, buff, 4, i2c);
}

int VL53L0X_read_byte(uint8_t deviceAddress, uint8_t index, uint8_t *data,
                      esphome::i2c::I2CBus *i2c) {
  return VL53L0X_read_multi(deviceAddress, index, data, 1, i2c);
}

int VL53L0X_read_word(uint8_t deviceAddress, uint8_t index, uint16_t *data,
                      esphome::i2c::I2CBus *i2c) {
  uint8_t buff[2];
  int r = VL53L0X_read_multi(deviceAddress, index, buff, 2, i2c);

  uint16_t tmp;
  tmp = buff[0];
  tmp <<= 8;
  tmp |= buff[1];
  *data = tmp;

  return r;
}

int VL53L0X_read_dword(uint8_t deviceAddress, uint8_t index, uint32_t *data,
                       esphome::i2c::I2CBus *i2c) {
  uint8_t buff[4];
  int r = VL53L0X_read_multi(deviceAddress, index, buff, 4, i2c);

  uint32_t tmp;
  tmp = buff[0];
  tmp <<= 8;
  tmp |= buff[1];
  tmp <<= 8;
  tmp |= buff[2];
  tmp <<= 8;
  tmp |= buff[3];

  *data = tmp;

  return r;
}
