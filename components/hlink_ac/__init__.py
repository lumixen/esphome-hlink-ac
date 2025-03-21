import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

hlink_ac_ns = cg.esphome_ns.namespace("hlink_ac")
HlinkAcComponent = hlink_ac_ns.class_("HlinkAcComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HlinkAcComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)