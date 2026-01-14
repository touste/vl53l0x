#include "vl53l0x_sensor.h"
#include "esphome/core/log.h"

/*
 * Most of the code in this integration is based on the VL53L0x library
 * by Pololu (Pololu Corporation), which in turn is based on the VL53L0X
 * API from ST.
 *
 * For more information about licensing, please view the included LICENSE.txt
 * file in the vl53l0x integration directory.
 */

namespace esphome {
namespace vl53l0x_lib {

static const char *const TAG = "vl53l0x_lib";

std::list<VL53L0XSensorMod *> VL53L0XSensorMod::
    vl53_sensors; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
bool VL53L0XSensorMod::enable_pin_setup_complete =
    false; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

VL53L0XSensorMod::VL53L0XSensorMod() {
  VL53L0XSensorMod::vl53_sensors.push_back(this);
}

void VL53L0XSensorMod::dump_config() {
  LOG_SENSOR("", "VL53L0X", this);
  LOG_UPDATE_INTERVAL(this);
  LOG_I2C_DEVICE(this);
  if (this->enable_pin_ != nullptr) {
    LOG_PIN("  Enable Pin: ", this->enable_pin_);
  }
  ESP_LOGCONFIG(TAG, "  Timeout: %u%s", this->timeout_us_,
                this->timeout_us_ > 0 ? "us" : " (no timeout)");
}

void VL53L0XSensorMod::setup() {
  ESP_LOGD(TAG, "'%s' - setup BEGIN", this->name_.c_str());

  if (!VL53L0XSensorMod::enable_pin_setup_complete) {
    for (auto &vl53_sensor : vl53_sensors) {
      if (vl53_sensor->enable_pin_ != nullptr) {
        vl53_sensor->enable_pin_->setup();
        vl53_sensor->enable_pin_->digital_write(false);
      }
    }
    VL53L0XSensorMod::enable_pin_setup_complete = true;
  }

  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->digital_write(true);
    delay(1);
  }

  uint8_t final_address = address_;
  this->set_i2c_address(0x29);

  reg(0x89) |= 0x01;
  reg(0x88) = 0x00;

  reg(0x80) = 0x01;
  reg(0xFF) = 0x01;
  reg(0x00) = 0x00;
  this->stop_variable_ = reg(0x91).get();

  reg(0x00) = 0x01;
  reg(0xFF) = 0x00;
  reg(0x80) = 0x00;
  reg(0x60) |= 0x12;

  if (this->long_range_)
    this->signal_rate_limit_ = 0.1;
  auto rate_value = static_cast<uint16_t>(signal_rate_limit_ * 128);
  write_byte_16(0x44, rate_value);

  reg(0x01) = 0xFF;

  // getSpadInfo()
  reg(0x80) = 0x01;
  reg(0xFF) = 0x01;
  reg(0x00) = 0x00;
  reg(0xFF) = 0x06;
  reg(0x83) |= 0x04;
  reg(0xFF) = 0x07;
  reg(0x81) = 0x01;
  reg(0x80) = 0x01;
  reg(0x94) = 0x6B;
  reg(0x83) = 0x00;

  this->timeout_start_us_ = micros();
  while (reg(0x83).get() == 0x00) {
    if (this->timeout_us_ > 0 &&
        ((uint16_t)(micros() - this->timeout_start_us_) > this->timeout_us_)) {
      ESP_LOGE(TAG, "'%s' - setup timeout", this->name_.c_str());
      this->mark_failed();
      return;
    }
    yield();
  }

  reg(0x83) = 0x01;
  uint8_t tmp = reg(0x92).get();
  uint8_t spad_count = tmp & 0x7F;
  bool spad_type_is_aperture = tmp & 0x80;

  reg(0x81) = 0x00;
  reg(0xFF) = 0x06;
  reg(0x83) &= ~0x04;
  reg(0xFF) = 0x01;
  reg(0x00) = 0x01;
  reg(0xFF) = 0x00;
  reg(0x80) = 0x00;

  uint8_t ref_spad_map[6] = {};
  this->read_bytes(0xB0, ref_spad_map, 6);

  reg(0xFF) = 0x01;
  reg(0x4F) = 0x00;
  reg(0x4E) = 0x2C;
  reg(0xFF) = 0x00;
  reg(0xB6) = 0xB4;

  uint8_t first_spad_to_enable = spad_type_is_aperture ? 12 : 0;
  uint8_t spads_enabled = 0;
  for (int i = 0; i < 48; i++) {
    uint8_t &val = ref_spad_map[i / 8];
    uint8_t mask = 1 << (i % 8);

    if (i < first_spad_to_enable || spads_enabled == spad_count) {
      val &= ~mask;
    } else if (val & mask) {
      spads_enabled += 1;
    }
  }

  this->write_bytes(0xB0, ref_spad_map, 6);

  reg(0xFF) = 0x01;
  reg(0x00) = 0x00;
  reg(0xFF) = 0x00;
  reg(0x09) = 0x00;
  reg(0x10) = 0x00;
  reg(0x11) = 0x00;
  reg(0x24) = 0x01;
  reg(0x25) = 0xFF;
  reg(0x75) = 0x00;
  reg(0xFF) = 0x01;
  reg(0x4E) = 0x2C;
  reg(0x48) = 0x00;
  reg(0x30) = 0x20;
  reg(0xFF) = 0x00;
  if (this->long_range_) {
    reg(0x30) = 0x07;
  } else {
    reg(0x30) = 0x09;
  }
  reg(0x54) = 0x00;
  reg(0x31) = 0x04;
  reg(0x32) = 0x03;
  reg(0x40) = 0x83;
  reg(0x46) = 0x25;
  reg(0x60) = 0x00;
  reg(0x27) = 0x00;
  reg(0x50) = 0x06;
  reg(0x51) = 0x00;
  reg(0x52) = 0x96;
  reg(0x56) = 0x08;
  if (this->long_range_) {
    reg(0x57) = 0x50;
  } else {
    reg(0x57) = 0x30;
  }
  reg(0x61) = 0x00;
  reg(0x62) = 0x00;
  reg(0x64) = 0x00;
  reg(0x65) = 0x00;
  reg(0x66) = 0xA0;
  reg(0xFF) = 0x01;
  reg(0x22) = 0x32;
  reg(0x47) = 0x14;
  reg(0x49) = 0xFF;
  reg(0x4A) = 0x00;
  reg(0xFF) = 0x00;
  reg(0x7A) = 0x0A;
  reg(0x7B) = 0x00;
  reg(0x78) = 0x21;
  reg(0xFF) = 0x01;
  reg(0x23) = 0x34;
  reg(0x42) = 0x00;
  reg(0x44) = 0xFF;
  reg(0x45) = 0x26;
  reg(0x46) = 0x05;
  reg(0x40) = 0x40;
  reg(0x0E) = 0x06;
  reg(0x20) = 0x1A;
  reg(0x43) = 0x40;
  reg(0xFF) = 0x00;
  reg(0x34) = 0x03;
  reg(0x35) = 0x44;
  reg(0xFF) = 0x01;
  reg(0x31) = 0x04;
  reg(0x4B) = 0x09;
  reg(0x4C) = 0x05;
  reg(0x4D) = 0x04;
  reg(0xFF) = 0x00;
  reg(0x44) = 0x00;
  reg(0x45) = 0x20;
  reg(0x47) = 0x08;
  if (this->long_range_) {
    reg(0x48) = 0x48;
  } else {
    reg(0x48) = 0x28;
  }
  reg(0x67) = 0x00;
  reg(0x70) = 0x04;
  reg(0x71) = 0x01;
  reg(0x72) = 0xFE;
  reg(0x76) = 0x00;
  reg(0x77) = 0x00;
  reg(0xFF) = 0x01;
  reg(0x0D) = 0x01;
  reg(0xFF) = 0x00;
  reg(0x80) = 0x01;
  reg(0x01) = 0xF8;
  reg(0xFF) = 0x01;
  reg(0x8E) = 0x01;
  reg(0x00) = 0x01;
  reg(0xFF) = 0x00;
  reg(0x80) = 0x00;

  reg(0x0A) = 0x04;
  reg(0x84) &= ~0x10;
  reg(0x0B) = 0x01;

  if (this->measurement_timing_budget_us_ == 0) {
    this->measurement_timing_budget_us_ =
        this->get_measurement_timing_budget_();
  }

  reg(0x01) = 0xE8;
  set_measurement_timing_budget_(measurement_timing_budget_us_);
  reg(0x01) = 0x01;

  if (!perform_single_ref_calibration_(0x40)) {
    ESP_LOGW(TAG, "1st reference calibration failed!");
    this->mark_failed();
    return;
  }
  reg(0x01) = 0x02;
  if (!perform_single_ref_calibration_(0x00)) {
    ESP_LOGW(TAG, "2nd reference calibration failed!");
    this->mark_failed();
    return;
  }
  reg(0x01) = 0xE8;

  reg(0x8A) = final_address & 0x7F;
  this->set_i2c_address(final_address);

  // Apply sensing mode configuration
  this->configure_sense_mode_();

  // Apply calibration settings
  this->apply_offset_calibration_();
  this->apply_crosstalk_compensation_();

  ESP_LOGD(TAG, "'%s' - setup END", this->name_.c_str());
}

void VL53L0XSensorMod::update() {
  if (this->initiated_read_ || this->waiting_for_interrupt_) {
    this->publish_state(NAN);
    ESP_LOGW(TAG, "%s - update called before prior reading complete",
             this->name_.c_str());
    return;
  }

  reg(0x80) = 0x01;
  reg(0xFF) = 0x01;
  reg(0x00) = 0x00;
  reg(0x91) = this->stop_variable_;
  reg(0x00) = 0x01;
  reg(0xFF) = 0x00;
  reg(0x80) = 0x00;

  reg(0x00) = 0x01;
  this->waiting_for_interrupt_ = false;
  this->initiated_read_ = true;
  this->read_start_time_ =
      millis(); // Track when we started for timeout recovery
}

void VL53L0XSensorMod::loop() {
  // Timeout recovery: if stuck in reading state for more than 5 seconds, reset
  const uint32_t READ_TIMEOUT_MS = 5000;
  if ((this->initiated_read_ || this->waiting_for_interrupt_) &&
      (millis() - this->read_start_time_ > READ_TIMEOUT_MS)) {
    ESP_LOGW(TAG, "'%s' - Reading timed out, resetting sensor state",
             this->name_.c_str());
    this->initiated_read_ = false;
    this->waiting_for_interrupt_ = false;
    // Clear any pending interrupts
    reg(0x0B) = 0x01;
    return;
  }

  if (this->initiated_read_) {
    uint8_t val = reg(0x00).get();
    if (val & 0x01) {
      // still waiting for measurement to start
    } else {
      // measurement started, now wait for interrupt
      this->initiated_read_ = false;
      this->waiting_for_interrupt_ = true;
    }
  }
  if (this->waiting_for_interrupt_) {
    uint8_t intmask = reg(0x13).get();
    if (intmask & 0x07) {
      uint16_t range_mm = 0;
      this->read_byte_16(0x14 + 10, &range_mm);

      // Read range status from result register (bits 3-5 of byte at 0x14)
      // Valid range status values are 0-5 (3 bits), so mask with 0x07
      uint8_t range_status = (reg(0x14).get() >> 3) & 0x07;

      reg(0x0B) = 0x01; // Clear interrupt
      this->waiting_for_interrupt_ = false;

      // Range status codes:
      // 0 = Valid measurement
      // 1 = Sigma fail (sigma too high)
      // 2 = Signal fail (signal too low)
      // 3 = Min range fail (too close)
      // 4 = Phase out of bounds (typically >= 8190mm)
      // 5 = Hardware fail

      if (range_mm >= 8190 || range_status == 4) {
        ESP_LOGD(TAG, "'%s' - Out of range", this->name_.c_str());
        this->publish_state(NAN);
        return;
      }

      if (range_status != 0) {
        const char *status_msg = "unknown";
        bool should_publish = true;
        switch (range_status) {
        case 1:
          status_msg = "Sigma fail";
          should_publish = !this->enable_sigma_check_;
          break;
        case 2:
          status_msg = "Signal fail";
          should_publish = !this->enable_signal_check_;
          break;
        case 3:
          status_msg = "Min range fail";
          break;
        case 5:
          status_msg = "Hardware fail";
          should_publish = false;
          break;
        }
        ESP_LOGD(TAG, "'%s' - Range status: %s (%d)", this->name_.c_str(),
                 status_msg, range_status);
        if (!should_publish) {
          this->publish_state(NAN);
          return;
        }
      }

      float range_m = range_mm / 1e3f;
      ESP_LOGD(TAG, "'%s' - Got distance %.3f m", this->name_.c_str(), range_m);
      this->publish_state(range_m);
    }
  }
}

uint32_t VL53L0XSensorMod::get_measurement_timing_budget_() {
  SequenceStepEnables enables{};
  SequenceStepTimeouts timeouts{};

  uint16_t start_overhead = 1910;
  uint16_t end_overhead = 960;
  uint16_t msrc_overhead = 660;
  uint16_t tcc_overhead = 590;
  uint16_t dss_overhead = 690;
  uint16_t pre_range_overhead = 660;
  uint16_t final_range_overhead = 550;

  uint32_t budget_us = start_overhead + end_overhead;

  get_sequence_step_enables_(&enables);
  get_sequence_step_timeouts_(&enables, &timeouts);

  if (enables.tcc)
    budget_us += (timeouts.msrc_dss_tcc_us + tcc_overhead);

  if (enables.dss) {
    budget_us += 2 * (timeouts.msrc_dss_tcc_us + dss_overhead);
  } else if (enables.msrc) {
    budget_us += (timeouts.msrc_dss_tcc_us + msrc_overhead);
  }

  if (enables.pre_range)
    budget_us += (timeouts.pre_range_us + pre_range_overhead);

  if (enables.final_range)
    budget_us += (timeouts.final_range_us + final_range_overhead);

  measurement_timing_budget_us_ = budget_us;
  return budget_us;
}

bool VL53L0XSensorMod::set_measurement_timing_budget_(uint32_t budget_us) {
  SequenceStepEnables enables{};
  SequenceStepTimeouts timeouts{};

  uint16_t start_overhead = 1320;
  uint16_t end_overhead = 960;
  uint16_t msrc_overhead = 660;
  uint16_t tcc_overhead = 590;
  uint16_t dss_overhead = 690;
  uint16_t pre_range_overhead = 660;
  uint16_t final_range_overhead = 550;

  uint32_t min_timing_budget = 20000;

  if (budget_us < min_timing_budget) {
    return false;
  }

  uint32_t used_budget_us = start_overhead + end_overhead;

  get_sequence_step_enables_(&enables);
  get_sequence_step_timeouts_(&enables, &timeouts);

  if (enables.tcc) {
    used_budget_us += (timeouts.msrc_dss_tcc_us + tcc_overhead);
  }

  if (enables.dss) {
    used_budget_us += 2 * (timeouts.msrc_dss_tcc_us + dss_overhead);
  } else if (enables.msrc) {
    used_budget_us += (timeouts.msrc_dss_tcc_us + msrc_overhead);
  }

  if (enables.pre_range) {
    used_budget_us += (timeouts.pre_range_us + pre_range_overhead);
  }

  if (enables.final_range) {
    used_budget_us += final_range_overhead;

    if (used_budget_us > budget_us) {
      return false;
    }

    uint32_t final_range_timeout_us = budget_us - used_budget_us;

    uint16_t final_range_timeout_mclks = timeout_microseconds_to_mclks_(
        final_range_timeout_us, timeouts.final_range_vcsel_period_pclks);

    if (enables.pre_range) {
      final_range_timeout_mclks += timeouts.pre_range_mclks;
    }

    write_byte_16(0x71, encode_timeout_(final_range_timeout_mclks));
    measurement_timing_budget_us_ = budget_us;
  }
  return true;
}

void VL53L0XSensorMod::get_sequence_step_enables_(
    SequenceStepEnables *enables) {
  uint8_t sequence_config = reg(0x01).get();
  enables->tcc = (sequence_config >> 4) & 0x1;
  enables->dss = (sequence_config >> 3) & 0x1;
  enables->msrc = (sequence_config >> 2) & 0x1;
  enables->pre_range = (sequence_config >> 6) & 0x1;
  enables->final_range = (sequence_config >> 7) & 0x1;
}

void VL53L0XSensorMod::get_sequence_step_timeouts_(
    SequenceStepEnables const *enables, SequenceStepTimeouts *timeouts) {
  timeouts->pre_range_vcsel_period_pclks =
      get_vcsel_pulse_period_(VCSEL_PERIOD_PRE_RANGE);

  timeouts->msrc_dss_tcc_mclks = reg(0x46).get() + 1;
  timeouts->msrc_dss_tcc_us = timeout_mclks_to_microseconds_(
      timeouts->msrc_dss_tcc_mclks, timeouts->pre_range_vcsel_period_pclks);

  uint16_t value;
  read_byte_16(0x51, &value);
  timeouts->pre_range_mclks = decode_timeout_(value);
  timeouts->pre_range_us = timeout_mclks_to_microseconds_(
      timeouts->pre_range_mclks, timeouts->pre_range_vcsel_period_pclks);

  timeouts->final_range_vcsel_period_pclks =
      get_vcsel_pulse_period_(VCSEL_PERIOD_FINAL_RANGE);

  read_byte_16(0x71, &value);
  timeouts->final_range_mclks = decode_timeout_(value);

  if (enables->pre_range) {
    timeouts->final_range_mclks -= timeouts->pre_range_mclks;
  }

  timeouts->final_range_us = timeout_mclks_to_microseconds_(
      timeouts->final_range_mclks, timeouts->final_range_vcsel_period_pclks);
}

uint8_t VL53L0XSensorMod::get_vcsel_pulse_period_(VcselPeriodType type) {
  uint8_t vcsel;
  if (type == VCSEL_PERIOD_PRE_RANGE) {
    vcsel = reg(0x50).get();
  } else if (type == VCSEL_PERIOD_FINAL_RANGE) {
    vcsel = reg(0x70).get();
  } else {
    return 255;
  }
  return (vcsel + 1) << 1;
}

uint32_t VL53L0XSensorMod::get_macro_period_(uint8_t vcsel_period_pclks) {
  return ((2304UL * vcsel_period_pclks * 1655UL) + 500UL) / 1000UL;
}

uint32_t
VL53L0XSensorMod::timeout_mclks_to_microseconds_(uint16_t timeout_period_mclks,
                                                 uint8_t vcsel_period_pclks) {
  uint32_t macro_period_ns = get_macro_period_(vcsel_period_pclks);
  return ((timeout_period_mclks * macro_period_ns) + (macro_period_ns / 2)) /
         1000;
}

uint32_t
VL53L0XSensorMod::timeout_microseconds_to_mclks_(uint32_t timeout_period_us,
                                                 uint8_t vcsel_period_pclks) {
  uint32_t macro_period_ns = get_macro_period_(vcsel_period_pclks);
  return (((timeout_period_us * 1000) + (macro_period_ns / 2)) /
          macro_period_ns);
}

uint16_t VL53L0XSensorMod::decode_timeout_(uint16_t reg_val) {
  uint8_t msb = (reg_val >> 8) & 0xFF;
  uint8_t lsb = (reg_val >> 0) & 0xFF;
  return (uint16_t(lsb) << msb) + 1;
}

uint16_t VL53L0XSensorMod::encode_timeout_(uint32_t timeout_mclks) {
  uint32_t ls_byte = 0;
  uint16_t ms_byte = 0;

  if (timeout_mclks <= 0)
    return 0;

  ls_byte = timeout_mclks - 1;

  while ((ls_byte & 0xFFFFFF00) > 0) {
    ls_byte >>= 1;
    ms_byte++;
  }

  return (ms_byte << 8) | (ls_byte & 0xFF);
}

bool VL53L0XSensorMod::perform_single_ref_calibration_(uint8_t vhv_init_byte) {
  reg(0x00) = 0x01 | vhv_init_byte;

  uint32_t start = millis();
  while ((reg(0x13).get() & 0x07) == 0) {
    if (millis() - start > 1000)
      return false;
    yield();
  }

  reg(0x0B) = 0x01;
  reg(0x00) = 0x00;

  return true;
}

void VL53L0XSensorMod::configure_sense_mode_() {
  // Apply sensing mode-specific configuration
  // Based on Adafruit VL53L0X library configSensor() function

  switch (this->sense_mode_) {
  case VL53L0X_SENSE_DEFAULT:
    // Default configuration - already applied during setup
    // Signal rate limit = 0.25, standard timing
    ESP_LOGD(TAG, "Sense mode: DEFAULT");
    break;

  case VL53L0X_SENSE_LONG_RANGE:
    ESP_LOGD(TAG, "Sense mode: LONG_RANGE");
    // Lower signal rate limit for better long range detection
    this->signal_rate_limit_ = 0.1f;
    {
      auto rate_value = static_cast<uint16_t>(this->signal_rate_limit_ * 128);
      write_byte_16(0x44, rate_value);
    }
    // Set timing budget to 33ms
    set_measurement_timing_budget_(33000);
    // Configure VCSEL pulse periods for long range
    set_vcsel_pulse_period_(VCSEL_PERIOD_PRE_RANGE, 18);
    set_vcsel_pulse_period_(VCSEL_PERIOD_FINAL_RANGE, 14);
    break;

  case VL53L0X_SENSE_HIGH_SPEED:
    ESP_LOGD(TAG, "Sense mode: HIGH_SPEED");
    // Faster measurements with reduced accuracy
    // Set timing budget to 20ms (minimum)
    set_measurement_timing_budget_(20000);
    break;

  case VL53L0X_SENSE_HIGH_ACCURACY:
    ESP_LOGD(TAG, "Sense mode: HIGH_ACCURACY");
    // Slower but more accurate measurements
    // Set timing budget to 200ms
    set_measurement_timing_budget_(200000);
    break;
  }
}

bool VL53L0XSensorMod::set_vcsel_pulse_period_(VcselPeriodType type,
                                               uint8_t period) {
  // Set VCSEL (laser) pulse period
  // period must be an even number; valid values are 8, 10, 12, 14, 16, 18

  uint8_t vcsel_period_reg = (period >> 1) - 1;

  SequenceStepEnables enables{};
  SequenceStepTimeouts timeouts{};
  get_sequence_step_enables_(&enables);
  get_sequence_step_timeouts_(&enables, &timeouts);

  if (type == VCSEL_PERIOD_PRE_RANGE) {
    // Set pre-range VCSEL period
    reg(0x50) = vcsel_period_reg;

    // Re-calculate and set pre-range timeout
    uint16_t new_pre_range_timeout_mclks =
        timeout_microseconds_to_mclks_(timeouts.pre_range_us, period);
    write_byte_16(0x51, encode_timeout_(new_pre_range_timeout_mclks));

    // Re-calculate and set MSRC timeout
    uint16_t new_msrc_timeout_mclks =
        timeout_microseconds_to_mclks_(timeouts.msrc_dss_tcc_us, period);
    reg(0x46) =
        (new_msrc_timeout_mclks > 256) ? 255 : (new_msrc_timeout_mclks - 1);

  } else if (type == VCSEL_PERIOD_FINAL_RANGE) {
    // Set final-range VCSEL period
    reg(0x70) = vcsel_period_reg;

    // Re-calculate and set final range timeout
    uint16_t new_final_range_timeout_mclks =
        timeout_microseconds_to_mclks_(timeouts.final_range_us, period);
    if (enables.pre_range) {
      new_final_range_timeout_mclks += timeouts.pre_range_mclks;
    }
    write_byte_16(0x71, encode_timeout_(new_final_range_timeout_mclks));
  }

  // Re-apply measurement timing budget
  set_measurement_timing_budget_(this->measurement_timing_budget_us_);

  return true;
}

void VL53L0XSensorMod::apply_offset_calibration_() {
  if (this->offset_calibration_um_ == 0) {
    return; // No calibration offset configured
  }

  // Offset register is in 10.2 format (units of mm/4)
  // Convert from micrometers to register value: um / 250
  int32_t cMaxOffset = 511000;  // +511mm in um
  int32_t cMinOffset = -512000; // -512mm in um
  int32_t offsetValue = this->offset_calibration_um_;

  // Clamp to valid range
  if (offsetValue > cMaxOffset)
    offsetValue = cMaxOffset;
  if (offsetValue < cMinOffset)
    offsetValue = cMinOffset;

  uint16_t encodedOffset;
  if (offsetValue >= 0) {
    encodedOffset = offsetValue / 250;
  } else {
    encodedOffset = 4096 + (offsetValue / 250); // 12-bit 2's complement
  }

  write_byte_16(0x24,
                encodedOffset); // VL53L0X_REG_ALGO_PART_TO_PART_RANGE_OFFSET_MM
  ESP_LOGD(TAG, "Applied offset calibration: %d um (reg: 0x%04X)",
           this->offset_calibration_um_, encodedOffset);
}

void VL53L0XSensorMod::apply_crosstalk_compensation_() {
  if (this->crosstalk_compensation_mcps_ <= 0.0f) {
    return; // No crosstalk compensation configured
  }

  // Crosstalk register is in 3.13 fixed-point format
  // Convert from MCPS (float) to fixed-point: value * 8192
  uint16_t xtalkValue =
      static_cast<uint16_t>(this->crosstalk_compensation_mcps_ * 8192.0f);

  write_byte_16(
      0x20, xtalkValue); // VL53L0X_REG_CROSSTALK_COMPENSATION_PEAK_RATE_MCPS
  ESP_LOGD(TAG, "Applied crosstalk compensation: %.3f MCPS (reg: 0x%04X)",
           this->crosstalk_compensation_mcps_, xtalkValue);
}

} // namespace vl53l0x_lib
} // namespace esphome
