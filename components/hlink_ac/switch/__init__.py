import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from ..climate import (
    CONF_HLINK_AC_ID,
    HlinkAc,
    hlink_ac_ns,
)

CODEOWNERS = ["@lumixen"]
RemoteLockSwitch = hlink_ac_ns.class_("RemoteLockSwitch", switch.Switch)

CONF_REMOTE_LOCK = "remote_lock"

ICON_REMOTE = "mdi:remote"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HLINK_AC_ID): cv.use_id(HlinkAc),
        cv.Optional(CONF_REMOTE_LOCK): switch.switch_schema(
            RemoteLockSwitch,
            icon=ICON_REMOTE,
            default_restore_mode="DISABLED",
        ),
    }
)

async def to_code(config):
    parent = await cg.get_variable(config[CONF_HLINK_AC_ID])

    for switch_type in [CONF_REMOTE_LOCK]:
        if conf := config.get(switch_type):
            sw_var = await switch.new_switch(conf)
            await cg.register_parented(sw_var, parent)
            cg.add(getattr(parent, f"set_{switch_type}_switch")(sw_var))