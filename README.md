# VL53L0X ESP-IDF Migration

This document describes the migration of the `vl53l0x_lib` custom ESPHome component from Arduino to ESP-IDF framework.

## Overview

The VL53L0X Time-of-Flight distance sensor component was originally built for Arduino framework. This migration enables it to work with ESP-IDF for better performance and reliability on ESP32 devices.

## Changes Made

### 1. ESPHome Configuration

**File:** `bad_vlib.yaml`

```yaml
# Changed from:
esp32:
  framework:
    type: arduino

# To:
esp32:
  framework:
    type: esp-idf
```

---

### 2. I2C Platform Layer

#### `vl53l0x_i2c_platform.h`
- Removed Arduino includes (`Arduino.h`, `Wire.h`)
- Changed `TwoWire *` → `esphome::i2c::I2CBus *`

```cpp
// Before
#include "Arduino.h"
#include "Wire.h"
VL53L0X_Error VL53L0X_WriteMulti(TwoWire *i2c, ...);

// After
namespace esphome { namespace i2c { class I2CBus; } }
VL53L0X_Error VL53L0X_WriteMulti(esphome::i2c::I2CBus *i2c, ...);
```

#### `vl53l0x_i2c_comms.cpp`
- Replaced Arduino `TwoWire` methods with ESPHome I2C methods

```cpp
// Before (Arduino)
i2c->beginTransmission(dev_addr);
i2c->write(index);
i2c->write(data, count);
i2c->endTransmission(true);

// After (ESPHome)
uint8_t buffer[count + 1];
buffer[0] = index;
memcpy(&buffer[1], pdata, count);
i2c->write(dev_addr, buffer, count + 1, true);
```

#### `vl53l0x_platform.h`
- Changed I2C pointer type in device structure

```cpp
// Before
TwoWire *i2c;

// After
esphome::i2c::I2CBus *i2c;
```

---

### 3. Sensor Implementation

#### `vl53l0x_sensor.h`
- Updated namespace: `vl53l0x` → `vl53l0x_lib`
- Renamed class: `VL53L0XSensor` → `VL53L0XSensorMod`
- **Fixed member initialization** (critical bug fix):

```cpp
// Before (uninitialized - caused stuck readings)
float signal_rate_limit_;
bool long_range_;

// After (properly initialized)
float signal_rate_limit_{0.25};
bool long_range_{false};
```

#### `vl53l0x_sensor.cpp`
- Replaced `delayMicroseconds()` with `delay()`
- Implemented full native ESPHome initialization sequence
- Added proper `update()` and `loop()` methods for measurements

---

### 4. Deleted Files
- `Adafruit_VL53L0X.cpp` - unused
- `Adafruit_VL53L0X.h` - unused

## Bug Fix

**Issue:** Sensor readings worked initially but got stuck after 2 readings.

**Root Cause:** Uninitialized member variables (`signal_rate_limit_`, `long_range_`, `measurement_timing_budget_us_`, `stop_variable_`) caused undefined behavior.

**Solution:** Added default initialization values in the header file.

## Verification

```
[17:46:07] 'Waschbecken Abstand' - Got distance 0.419 m
[17:46:12] 'Waschbecken Abstand' - Got distance 0.304 m
[17:46:27] 'Waschbecken Abstand' - Got distance 0.148 m
[17:46:52] 'Waschbecken Abstand' - Got distance 0.029 m
```

## Usage

```yaml
esphome:
  name: my-device

esp32:
  board: esp32dev
  framework:
    type: esp-idf

external_components:
  - source: my_components

i2c:
  sda: 21
  scl: 22

sensor:
  - platform: vl53l0x_lib
    name: "Distance Sensor"
    update_interval: 5s
```
