import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi, sh1122_base
from esphome.const import CONF_DC_PIN, CONF_ID, CONF_LAMBDA, CONF_PAGES

CODEOWNERS = ["@pl4nkton"]

AUTO_LOAD = ["sh1122_base"]
DEPENDENCIES = ["spi"]

sh1122_spi = cg.esphome_ns.namespace("sh1122_spi")
SPISH1122 = sh1122_spi.class_("SPISH1122", sh1122_base.SH1122, spi.SPIDevice)

CONFIG_SCHEMA = cv.All(
    sh1122_base.SH1122_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SPISH1122),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema()),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await sh1122_base.setup_sh1122(var, config)
    await spi.register_spi_device(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))
