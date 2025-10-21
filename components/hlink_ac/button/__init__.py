import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from ..climate import (
    CONF_HLINK_AC_ID,
    HlinkAc,
    hlink_ac_ns,
)

CODEOWNERS = ["@lumixen"]
ResetAirFilterCleanWarningButton = hlink_ac_ns.class_(
    "ResetAirFilterCleanWarningButton", button.Button
)

CONF_RESET_AIR_FILTER_CLEAN_WARNING = "reset_air_filter_warning"

# Additional icons
ICON_AIR_FILTER = "mdi:air-filter"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HLINK_AC_ID): cv.use_id(HlinkAc),
        cv.Optional(CONF_RESET_AIR_FILTER_CLEAN_WARNING): button.button_schema(
            ResetAirFilterCleanWarningButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_AIR_FILTER,
        ),
    }
)


async def to_code(config):
    for button_type in [CONF_RESET_AIR_FILTER_CLEAN_WARNING]:
        if conf := config.get(button_type):
            btn = await button.new_button(conf)
            await cg.register_parented(btn, config[CONF_HLINK_AC_ID])
