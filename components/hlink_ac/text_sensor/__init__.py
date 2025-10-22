from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from ..climate import (
    CONF_HLINK_AC_ID,
    HlinkAc,
    hlink_ac_ns,
)

CODEOWNERS = ["@lumixen"]
TextSensorTypeEnum = hlink_ac_ns.enum("TextSensorType", True)

MODEL_NAME = "model_name"
DEBUG = "debug"
DEBUG_DISCOVERY = "debug_discovery"

CONF_ADDRESS = "address"

ICON_BUG = "mdi:bug"
ICON_INFORMATION = "mdi:information-outline"

TEXT_SENSOR_TYPES = {
    MODEL_NAME: text_sensor.text_sensor_schema(
        icon=ICON_INFORMATION,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    DEBUG: text_sensor.text_sensor_schema(
        icon=ICON_BUG,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ).extend(
        {
            cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
        }
    ),
    DEBUG_DISCOVERY: text_sensor.text_sensor_schema(
        icon=ICON_BUG,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HLINK_AC_ID): cv.use_id(HlinkAc),
    }
).extend({cv.Optional(type): schema for type, schema in TEXT_SENSOR_TYPES.items()})

TEXT_SENSOR_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HLINK_AC_ID): cv.use_id(HlinkAc),
        cv.Required(CONF_ID): cv.use_id(text_sensor.TextSensor),
    }
)

StartDebugDiscoveryAction = hlink_ac_ns.class_("StartDebugDiscovery", automation.Action)
StopDebugDiscoveryAction = hlink_ac_ns.class_("StopDebugDiscovery", automation.Action)


@automation.register_action(
    "text_sensor.hlink_ac.start_debug_discovery",
    StartDebugDiscoveryAction,
    TEXT_SENSOR_ACTION_SCHEMA,
)
@automation.register_action(
    "text_sensor.hlink_ac.stop_debug_discovery",
    StopDebugDiscoveryAction,
    TEXT_SENSOR_ACTION_SCHEMA,
)
async def debug_discovery_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_HLINK_AC_ID])
    return var


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HLINK_AC_ID])
    for type_ in TEXT_SENSOR_TYPES:
        if conf := config.get(type_):
            sens = await text_sensor.new_text_sensor(conf)
            if type_ == DEBUG:
                cg.add(parent.set_debug_text_sensor(conf[CONF_ADDRESS], sens))
            elif type_ == DEBUG_DISCOVERY:
                cg.add(parent.set_debug_discovery_text_sensor(sens))
            else:
                sensor_type = getattr(TextSensorTypeEnum, type_.upper())
                cg.add(parent.set_text_sensor(sensor_type, sens))
