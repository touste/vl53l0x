#include "vl53l0x_sensor.h"
#include "esphome/core/log.h"

/*
 * Most of the code in this integration is based on the VL53L0x library
 * by Pololu (Pololu Corporation), which in turn is based on the VL53L0X
 * API from ST.
 *
 * For more information about licensing, please view the included LICENSE.txt file
 * in the vl53l0x integration directory.
 */

namespace esphome {
namespace vl53l0x {

static const char *const TAG = "vl53l0x";

std::list<VL53L0XSensor *> VL53L0XSensor::vl53_sensors;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
bool VL53L0XSensor::enable_pin_setup_complete = false;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

VL53L0XSensor::VL53L0XSensor() { VL53L0XSensor::vl53_sensors.push_back(this); }

void VL53L0XSensor::dump_config() {
  LOG_SENSOR("", "VL53L0X", this);
  LOG_UPDATE_INTERVAL(this);
  LOG_I2C_DEVICE(this);
  if (this->enable_pin_ != nullptr) {
    LOG_PIN("  Enable Pin: ", this->enable_pin_);
  }
  ESP_LOGCONFIG(TAG, "  Timeout: %u%s", this->timeout_us_, this->timeout_us_ > 0 ? "us" : " (no timeout)");
}

void VL53L0XSensor::calibrate_() {


}

void VL53L0XSensor::setup() {
  ESP_LOGD(TAG, "'%s' - setup BEGIN", this->name_.c_str());

  if (!esphome::vl53l0x::VL53L0XSensor::enable_pin_setup_complete) {
    for (auto &vl53_sensor : vl53_sensors) {
      if (vl53_sensor->enable_pin_ != nullptr) {
        // Set enable pin as OUTPUT and disable the enable pin to force vl53 to HW Standby mode
        vl53_sensor->enable_pin_->setup();
        vl53_sensor->enable_pin_->digital_write(false);
      }
    }
    esphome::vl53l0x::VL53L0XSensor::enable_pin_setup_complete = true;
  }

  if (this->enable_pin_ != nullptr) {
    // Enable the enable pin to cause FW boot (to get back to 0x29 default address)
    this->enable_pin_->digital_write(true);
    delayMicroseconds(100);
  }

  // Save the i2c address we want and force it to use the default 0x29
  // until we finish setup, then re-address to final desired address.
  uint8_t final_address = address_;
  this->set_i2c_address(0x29);

  this->calibrate_();

  // Set the sensor to the desired final address
  // The following is different for VL53L0X vs VL53L1X
  // I2C_SXXXX_DEVICE_ADDRESS = 0x8A for VL53L0X
  // I2C_SXXXX__DEVICE_ADDRESS = 0x0001 for VL53L1X
  reg(0x8A) = final_address & 0x7F;
  this->set_i2c_address(final_address);
  
  uint8_t status_reg = reg(0x14).get() >> 3;
  if (status_reg)
    ESP_LOGW(TAG, "Status register contains error: %d", status_reg);

  ESP_LOGD(TAG, "'%s' - setup END", this->name_.c_str());
}

void VL53L0XSensor::update() {


  // initiate single shot measurement

  // wait for timeout
}

void VL53L0XSensor::loop() {
  if (this->initiated_read_) {
    if (this->nerviger_bug_)
      ESP_LOGW(TAG, "'%s' - Read Reg 0", this->name_.c_str());
    if (reg(0x00).get() & 0x01) {
      // waiting
    } else {
      // done
      // wait until reg(0x13) & 0x07 is set
      this->initiated_read_ = false;
      this->waiting_for_interrupt_ = true;
    }
  }
  if (this->waiting_for_interrupt_) {
    uint8_t intmask = reg(0x13).get();
    //if (this->nerviger_bug_)
    //  ESP_LOGW(TAG, "'%s' - Waiting Int %d", this->name_.c_str(), intmask);
    if (intmask & 0x07) {
      uint16_t range_mm = 0;
      this->read_byte_16(0x14 + 10, &range_mm);
      reg(0x0B) = 0x01;
      this->waiting_for_interrupt_ = false;

      if (range_mm >= 8190) {
        ESP_LOGD(TAG, "'%s' - Distance is out of range, please move the target closer", this->name_.c_str());
        this->publish_state(NAN);
        return;
      }

      float range_m = range_mm / 1e3f;
      ESP_LOGD(TAG, "'%s' - Got distance %.3f m", this->name_.c_str(), range_m);
      this->publish_state(range_m);
    }
    else if (reg(0x14).get() & 0x01) {
      ESP_LOGW(TAG, "'%s' - Waiting reg 14 int", this->name_.c_str());
      this->calibrate_();
    }
    /*else if (intmask & 0x18) {
      ESP_LOGW(TAG, "'%s' - Range Error", this->name_.c_str());
      reg(0x0B) = 0x01;
      this->publish_state(NAN);
      this->waiting_for_interrupt_ = false;
    }*/
  }
}


}  // namespace vl53l0x
}  // namespace esphome
