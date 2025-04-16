import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uart
from esphome.const import CONF_ID

CODEOWNERS = ["@lumixen"]
DEPENDENCIES = ["climate", "uart"]

hlink_ac_ns = cg.esphome_ns.namespace("hlink_ac")
HlinkAc = hlink_ac_ns.class_("HlinkAc", cg.Component, uart.UARTDevice, climate.Climate)

CONF_HLINK_AC_ID = "hlink_ac_id"

CONFIG_SCHEMA = cv.All(
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HlinkAc),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)