import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from ..climate import (
    CONF_HLINK_AC_ID,
    HlinkAc,
    hlink_ac_ns,
)

CODEOWNERS = ["@lumixen"]
BinarySensorTypeEnum = hlink_ac_ns.enum("BinarySensorType", True)

CONF_AIR_FILTER_WARNING = "air_filter_warning"

ICON_AIR_FILTER_WARNING = "mdi:air-filter"

SENSOR_TYPES = {
    CONF_AIR_FILTER_WARNING: binary_sensor.binary_sensor_schema(
        icon=ICON_AIR_FILTER_WARNING,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HLINK_AC_ID): cv.use_id(HlinkAc),
    }
).extend({cv.Optional(type): schema for type, schema in SENSOR_TYPES.items()})


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HLINK_AC_ID])

    for type_ in SENSOR_TYPES:
        if conf := config.get(type_):
            sens = await binary_sensor.new_binary_sensor(conf)
            binary_sensor_type = getattr(BinarySensorTypeEnum, type_.upper())
            cg.add(parent.set_binary_sensor(binary_sensor_type, sens))
