import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
    ICON_ARROW_EXPAND_VERTICAL,
    CONF_ADDRESS,
    CONF_TIMEOUT,
    CONF_ENABLE_PIN,
)
from esphome import pins

DEPENDENCIES = ["i2c"]

vl53l0x_lib_ns = cg.esphome_ns.namespace("vl53l0x_lib")
VL53L0XSensorMod = vl53l0x_lib_ns.class_(
    "VL53L0XSensorMod", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

VL53L0XSenseMode = vl53l0x_lib_ns.enum("VL53L0XSenseMode")
SENSE_MODES = {
    "default": VL53L0XSenseMode.VL53L0X_SENSE_DEFAULT,
    "long_range": VL53L0XSenseMode.VL53L0X_SENSE_LONG_RANGE,
    "high_speed": VL53L0XSenseMode.VL53L0X_SENSE_HIGH_SPEED,
    "high_accuracy": VL53L0XSenseMode.VL53L0X_SENSE_HIGH_ACCURACY,
}

CONF_SIGNAL_RATE_LIMIT = "signal_rate_limit"
CONF_LONG_RANGE = "long_range"
CONF_SENSE_MODE = "sense_mode"
CONF_ENABLE_SIGMA_CHECK = "enable_sigma_check"
CONF_ENABLE_SIGNAL_CHECK = "enable_signal_check"
CONF_TIMING_BUDGET = "timing_budget"
CONF_OFFSET_CALIBRATION = "offset_calibration"
CONF_CROSSTALK_COMPENSATION = "crosstalk_compensation"
CONF_ENABLE_TEMPERATURE_RECAL = "enable_temperature_recal"
CONF_READING_TIMEOUT = "reading_timeout"


def check_keys(obj):
    if obj[CONF_ADDRESS] != 0x29 and CONF_ENABLE_PIN not in obj:
        msg = "Address other then 0x29 requires enable_pin definition to allow sensor\r"
        msg += "re-addressing. Also if you have more then one VL53 device on the same\r"
        msg += "i2c bus, then all VL53 devices must have enable_pin defined."
        raise cv.Invalid(msg)
    return obj


def check_timeout(value):
    value = cv.positive_time_period_microseconds(value)
    if value.total_seconds > 60:
        raise cv.Invalid("Maximum timeout can not be greater then 60 seconds")
    return value


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        VL53L0XSensorMod,
        unit_of_measurement=UNIT_METER,
        icon=ICON_ARROW_EXPAND_VERTICAL,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional(CONF_SIGNAL_RATE_LIMIT, default=0.25): cv.float_range(
                min=0.0, max=512.0, min_included=False, max_included=False
            ),
            cv.Optional(CONF_LONG_RANGE, default=False): cv.boolean,
            cv.Optional(CONF_SENSE_MODE, default="default"): cv.enum(
                SENSE_MODES, lower=True
            ),
            cv.Optional(CONF_ENABLE_SIGMA_CHECK, default=True): cv.boolean,
            cv.Optional(CONF_ENABLE_SIGNAL_CHECK, default=True): cv.boolean,
            cv.Optional(CONF_TIMING_BUDGET): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(min=cv.TimePeriod(microseconds=20000)),
            ),
            cv.Optional(CONF_OFFSET_CALIBRATION): cv.int_range(min=-512000, max=511000),
            cv.Optional(CONF_CROSSTALK_COMPENSATION): cv.float_range(min=0.0, max=10.0),
            cv.Optional(CONF_ENABLE_TEMPERATURE_RECAL, default=False): cv.boolean,
            cv.Optional(CONF_READING_TIMEOUT, default="5s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(milliseconds=1000), max=cv.TimePeriod(milliseconds=60000)),
            ),
            cv.Optional(CONF_TIMEOUT, default="10ms"): check_timeout,
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x29)),
    check_keys,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_signal_rate_limit(config[CONF_SIGNAL_RATE_LIMIT]))
    cg.add(var.set_long_range(config[CONF_LONG_RANGE]))
    cg.add(var.set_sense_mode(config[CONF_SENSE_MODE]))
    cg.add(var.set_enable_sigma_check(config[CONF_ENABLE_SIGMA_CHECK]))
    cg.add(var.set_enable_signal_check(config[CONF_ENABLE_SIGNAL_CHECK]))
    if CONF_TIMING_BUDGET in config:
        cg.add(var.set_timing_budget(config[CONF_TIMING_BUDGET]))
    if CONF_OFFSET_CALIBRATION in config:
        cg.add(var.set_offset_calibration(config[CONF_OFFSET_CALIBRATION]))
    if CONF_CROSSTALK_COMPENSATION in config:
        cg.add(var.set_crosstalk_compensation(config[CONF_CROSSTALK_COMPENSATION]))
    cg.add(var.set_enable_temperature_recal(config[CONF_ENABLE_TEMPERATURE_RECAL]))
    cg.add(var.set_reading_timeout_ms(config[CONF_READING_TIMEOUT]))
    cg.add(var.set_timeout_us(config[CONF_TIMEOUT]))

    if CONF_ENABLE_PIN in config:
        enable = await cg.gpio_pin_expression(config[CONF_ENABLE_PIN])
        cg.add(var.set_enable_pin(enable))

    await i2c.register_i2c_device(var, config)

