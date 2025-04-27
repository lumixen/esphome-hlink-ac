import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from ..climate import (
    CONF_HLINK_AC_ID,
    HlinkAc,
    hlink_ac_ns,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@lumixen"]
TextSensorTypeEnum = hlink_ac_ns.enum("TextSensorType", True)

MODEL_NAME = "model_name"
DEBUG = "debug"

CONF_ADDRESS = "address"

# ICON_BUG = "mdi:bug"
ICON_INFORMATION = "mdi:information-outline"

TEXT_SENSOR_TYPES = {
    MODEL_NAME: text_sensor.text_sensor_schema(
        icon=ICON_INFORMATION,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    DEBUG: text_sensor.TEXT_SENSOR_SCHEMA.extend(
        {
            cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
        }
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HLINK_AC_ID): cv.use_id(HlinkAc),
    }
).extend({cv.Optional(type): schema for type, schema in TEXT_SENSOR_TYPES.items()})


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HLINK_AC_ID])
    _LOGGER.info("Parent: %s", parent)
    for type_ in TEXT_SENSOR_TYPES:
        if conf := config.get(type_):
            sens = await text_sensor.new_text_sensor(conf)
            if type_ == DEBUG:
                cg.add(parent.set_debug_text_sensor(conf[CONF_ADDRESS], sens))
            else:
                sensor_type = getattr(TextSensorTypeEnum, type_.upper())
                cg.add(parent.set_text_sensor(sensor_type, sens))