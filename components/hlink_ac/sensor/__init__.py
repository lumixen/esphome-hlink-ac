import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_RADIATOR,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
from ..climate import (
    CONF_HLINK_AC_ID,
    HlinkAc,
    hlink_ac_ns,
)

CODEOWNERS = ["@lumixen"]
SensorTypeEnum = hlink_ac_ns.enum("SensorType", True)

OUTDOOR_TEMPERATURE = "outdoor_temperature"
AUTO_TARGET_TEMP_OFFSET = "auto_target_temp_offset"

ICON_THERMOMETER_AUTO = "mdi:thermometer-auto"

SENSOR_TYPES = {
    OUTDOOR_TEMPERATURE: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        icon=ICON_RADIATOR,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    AUTO_TARGET_TEMP_OFFSET: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        icon=ICON_THERMOMETER_AUTO,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HLINK_AC_ID): cv.use_id(HlinkAc),
    }
).extend({cv.Optional(type_): schema for type_, schema in SENSOR_TYPES.items()})


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HLINK_AC_ID])

    for type_ in SENSOR_TYPES:
        if conf := config.get(type_):
            sens = await sensor.new_sensor(conf)
            sensor_type = getattr(SensorTypeEnum, type_.upper())
            cg.add(parent.set_sensor(sensor_type, sens))
