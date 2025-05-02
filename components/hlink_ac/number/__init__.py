import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import ENTITY_CATEGORY_CONFIG, UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE
from ..climate import (
    CONF_HLINK_AC_ID,
    HlinkAc,
    hlink_ac_ns,
)

CODEOWNERS = ["@lumixen"]

TEMPERATURE_AUTO_OFFSET = "temperature_auto_offset"
TEMPERATURE_AUTO_OFFSET_ICON = "mdi:temperature-celsius"

TemperatureAutoOffsetNumber = hlink_ac_ns.class_(
    "TemperatureAutoOffsetNumber", number.Number
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HLINK_AC_ID): cv.use_id(HlinkAc),
        cv.Optional(TEMPERATURE_AUTO_OFFSET): number.number_schema(
            TemperatureAutoOffsetNumber,
            icon=TEMPERATURE_AUTO_OFFSET_ICON,
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HLINK_AC_ID])
    if temperature_auto_offset_config := config.get(TEMPERATURE_AUTO_OFFSET):
        offset_number = await number.new_number(
            temperature_auto_offset_config,
            min_value=-3,
            max_value=3,
            step=1,
        )
        await cg.register_parented(offset_number, config[CONF_HLINK_AC_ID])
        cg.add(parent.set_temperature_offset_number(offset_number))
