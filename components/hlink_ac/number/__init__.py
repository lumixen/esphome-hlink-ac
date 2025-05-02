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

AUTO_TARGET_TEMP_OFFSET = "auto_target_temperature_offset"
AUTO_TEMP_OFFSET_ICON = "mdi:temperature-celsius"

AutoTargetTemperatureOffsetNumber = hlink_ac_ns.class_(
    "AutoTargetTemperatureOffsetNumber", number.Number
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HLINK_AC_ID): cv.use_id(HlinkAc),
        cv.Optional(AUTO_TARGET_TEMP_OFFSET): number.number_schema(
            AutoTargetTemperatureOffsetNumber,
            icon=AUTO_TEMP_OFFSET_ICON,
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HLINK_AC_ID])
    if auto_temp_offset := config.get(AUTO_TARGET_TEMP_OFFSET):
        offset_number = await number.new_number(
            auto_temp_offset,
            min_value=-3,
            max_value=3,
            step=1,
        )
        await cg.register_parented(offset_number, config[CONF_HLINK_AC_ID])
        cg.add(parent.set_temperature_offset_number(offset_number))
