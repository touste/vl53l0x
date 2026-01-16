# VL53L0X Enhanced ESPHome Component

An enhanced VL53L0X Time-of-Flight distance sensor component for ESPHome with ESP-IDF framework support.

## Features

- ✅ **ESP-IDF Native** - No Arduino dependencies
- ✅ **Sensing Modes** - Default, Long Range, High Speed, High Accuracy presets
- ✅ **Range Status Validation** - Sigma, signal, min range, and hardware fail detection
- ✅ **Offset Calibration** - Factory offset correction (-512mm to +511mm)
- ✅ **Crosstalk Compensation** - Cover glass crosstalk correction
- ✅ **Configurable Timing Budget** - Trade speed for accuracy (20ms to 200ms+)
- ✅ **Automatic Recovery** - Configurable timeout with hard reset on sensor deadlock

## Installation

Add to your ESPHome configuration:

```yaml
external_components:
  - source: github://zaolin/vl53l0x@master
    components: [vl53l0x_lib]
```

Or for local development:

```yaml
external_components:
  - source: components
    components: [vl53l0x_lib]
```

## Basic Usage

```yaml
i2c:
  sda: 21
  scl: 22

sensor:
  - platform: vl53l0x_lib
    name: "Distance Sensor"
    update_interval: 1s
```

## Full Configuration

```yaml
sensor:
  - platform: vl53l0x_lib
    name: "Distance Sensor"
    
    # Sensing mode presets
    sense_mode: default  # default | long_range | high_speed | high_accuracy
    
    # Quality checks (default: enabled)
    enable_sigma_check: true
    enable_signal_check: true
    
    # Timing budget (optional, overrides sense_mode timing)
    timing_budget: 33ms  # 20ms minimum, higher = more accurate
    
    # Calibration (optional)
    offset_calibration: 0        # micrometers, range: -512000 to +511000
    crosstalk_compensation: 0.0  # MCPS, range: 0.0 to 10.0
    
    # Timeout recovery (optional)
    reading_timeout: 5s          # Hard reset timeout, range: 1s to 60s
    
    # Native ESPHome options
    signal_rate_limit: 0.25
    long_range: false
    timeout: 10ms
    update_interval: 1s
    address: 0x29
    # enable_pin: GPIO5  # Required if using non-default address
```

## Sensing Modes

| Mode | Timing Budget | Signal Rate | Best For |
|------|---------------|-------------|----------|
| `default` | 33ms | 0.25 | General use |
| `long_range` | 33ms | 0.1 | Extended range (up to 2m) |
| `high_speed` | 20ms | 0.25 | Fast updates, reduced accuracy |
| `high_accuracy` | 200ms | 0.25 | Maximum precision |

## Calibration Guide

### Offset Calibration

If your sensor consistently reads too high or too low, apply offset calibration:

```yaml
offset_calibration: 25000  # +25mm correction in micrometers
```

Measure a known distance at ~100mm and calculate the offset.

### Crosstalk Compensation

If using a cover glass, crosstalk compensation improves accuracy:

```yaml
crosstalk_compensation: 0.5  # MCPS value from calibration
```

## Range Status Codes

The component validates range status and logs errors:

| Status | Meaning | Action |
|--------|---------|--------|
| 0 | Valid | Published |
| 1 | Sigma fail | Filtered if `enable_sigma_check: true` |
| 2 | Signal fail | Filtered if `enable_signal_check: true` |
| 3 | Min range fail | Logged, still published |
| 4 | Phase out of bounds | Published as NAN (out of range) |
| 5 | Hardware fail | Published as NAN |

## Timeout Recovery

The sensor can enter a permanent deadlock state on I2C/hardware failures. The component includes automatic recovery with configurable timeout:

```yaml
reading_timeout: 5s  # Default value, range: 1s to 60s
```

**How it works:**
- If the sensor gets stuck in a reading state for longer than `reading_timeout`, a full hardware reset is performed
- The timeout triggers a complete sensor reinitialization via `setup()`
- Default timeout is 5 seconds, adjustable between 1-60 seconds

## Multiple Sensors

For multiple sensors on the same I2C bus, use `enable_pin` for address management:

```yaml
sensor:
  - platform: vl53l0x_lib
    name: "Sensor 1"
    address: 0x30
    enable_pin: GPIO5
    
  - platform: vl53l0x_lib
    name: "Sensor 2"  
    address: 0x31
    enable_pin: GPIO4
```

## Requirements

- ESP32 with ESP-IDF framework
- I2C connected VL53L0X sensor

## License

Based on Pololu VL53L0X library and STMicroelectronics API.
