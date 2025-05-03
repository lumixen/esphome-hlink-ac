from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uart
from esphome.components.climate import (
    CONF_CURRENT_TEMPERATURE,
    ClimateMode,
    ClimateSwingMode,
    ClimateFanMode,
)
from esphome.const import (
    CONF_ADDRESS,
    CONF_DATA,
    CONF_SUPPORTED_MODES,
    CONF_SUPPORTED_SWING_MODES,
    CONF_SUPPORTED_FAN_MODES,
    CONF_ID,
    CONF_VISUAL,
    CONF_MIN_TEMPERATURE,
    CONF_MAX_TEMPERATURE,
    CONF_TEMPERATURE_STEP,
    CONF_TARGET_TEMPERATURE,
)

CODEOWNERS = ["@lumixen"]
DEPENDENCIES = ["climate", "uart"]

hlink_ac_ns = cg.esphome_ns.namespace("hlink_ac")
HlinkAc = hlink_ac_ns.class_("HlinkAc", cg.Component, uart.UARTDevice, climate.Climate)

CONF_HLINK_AC_ID = "hlink_ac_id"

PROTOCOL_MIN_TEMPERATURE = 16.0
PROTOCOL_MAX_TEMPERATURE = 32.0
PROTOCOL_TARGET_TEMPERATURE_STEP = 1.0
PROTOCOL_CURRENT_TEMPERATURE_STEP = 1.0

SUPPORT_HVAC_ACTIONS = "hvac_actions"

SUPPORTED_CLIMATE_MODES_OPTIONS = {
    "OFF": ClimateMode.CLIMATE_MODE_OFF,
    "COOL": ClimateMode.CLIMATE_MODE_COOL,
    "HEAT": ClimateMode.CLIMATE_MODE_HEAT,
    "DRY": ClimateMode.CLIMATE_MODE_DRY,
    "FAN_ONLY": ClimateMode.CLIMATE_MODE_FAN_ONLY,
    "AUTO": ClimateMode.CLIMATE_MODE_AUTO,
}

SUPPORTED_SWING_MODES_OPTIONS = {
    "OFF": ClimateSwingMode.CLIMATE_SWING_OFF,
    "VERTICAL": ClimateSwingMode.CLIMATE_SWING_VERTICAL,
}

SUPPORTED_FAN_MODES_OPTIONS = {
    "AUTO": ClimateFanMode.CLIMATE_FAN_AUTO,
    "LOW": ClimateFanMode.CLIMATE_FAN_LOW,
    "MEDIUM": ClimateFanMode.CLIMATE_FAN_MEDIUM,
    "HIGH": ClimateFanMode.CLIMATE_FAN_HIGH,
    "QUIET": ClimateFanMode.CLIMATE_FAN_QUIET,
}

HlinkAcSendHlinkCmd = hlink_ac_ns.class_("HlinkAcSendHlinkCmd", automation.Action)


@automation.register_action(
    "hlink_ac.send_hlink_cmd",
    HlinkAcSendHlinkCmd,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HlinkAc),
            cv.Required(CONF_ADDRESS): cv.templatable(cv.string),
            cv.Required(CONF_DATA): cv.templatable(cv.string),
        }
    ),
)
async def send_hlink_cmd_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    address_template = await cg.templatable(config[CONF_ADDRESS], args, cg.std_string)
    data_template = await cg.templatable(config[CONF_DATA], args, cg.std_string)

    cg.add(var.set_address(address_template))
    cg.add(var.set_data(data_template))

    return var


def validate_visual(config):
    if CONF_VISUAL in config:
        visual_config = config[CONF_VISUAL]
        if CONF_MIN_TEMPERATURE in visual_config:
            min_temp = visual_config[CONF_MIN_TEMPERATURE]
            if min_temp < PROTOCOL_MIN_TEMPERATURE:
                raise cv.Invalid(
                    f"Configured visual minimum temperature {min_temp} is lower than supported by H-Link protocol is {PROTOCOL_MIN_TEMPERATURE}"
                )
        else:
            config[CONF_VISUAL][CONF_MIN_TEMPERATURE] = PROTOCOL_MIN_TEMPERATURE
        if CONF_MAX_TEMPERATURE in visual_config:
            max_temp = visual_config[CONF_MAX_TEMPERATURE]
            if max_temp > PROTOCOL_MAX_TEMPERATURE:
                raise cv.Invalid(
                    f"Configured visual maximum temperature {max_temp} is higher than supported by H-Link protocol is {PROTOCOL_MAX_TEMPERATURE}"
                )
        else:
            config[CONF_VISUAL][CONF_MAX_TEMPERATURE] = PROTOCOL_MAX_TEMPERATURE
        if CONF_TEMPERATURE_STEP in visual_config:
            temp_step = config[CONF_VISUAL][CONF_TEMPERATURE_STEP][
                CONF_TARGET_TEMPERATURE
            ]
            if temp_step % 1 != 0:
                raise cv.Invalid(
                    f"Configured visual temperature step {temp_step} is wrong, it should be a multiple of 1"
                )
        else:
            config[CONF_VISUAL][CONF_TEMPERATURE_STEP] = {
                CONF_TARGET_TEMPERATURE: PROTOCOL_TARGET_TEMPERATURE_STEP,
                CONF_CURRENT_TEMPERATURE: PROTOCOL_CURRENT_TEMPERATURE_STEP,
            }
    else:
        config[CONF_VISUAL] = {
            CONF_MIN_TEMPERATURE: PROTOCOL_MIN_TEMPERATURE,
            CONF_MAX_TEMPERATURE: PROTOCOL_MAX_TEMPERATURE,
            CONF_TEMPERATURE_STEP: {
                CONF_TARGET_TEMPERATURE: PROTOCOL_TARGET_TEMPERATURE_STEP,
                CONF_CURRENT_TEMPERATURE: PROTOCOL_CURRENT_TEMPERATURE_STEP,
            },
        }
    return config


CONFIG_SCHEMA = cv.All(
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HlinkAc),
            cv.Optional(
                CONF_SUPPORTED_MODES,
                default=list(SUPPORTED_CLIMATE_MODES_OPTIONS.keys()),
            ): cv.ensure_list(cv.enum(SUPPORTED_CLIMATE_MODES_OPTIONS, upper=True)),
            cv.Optional(
                CONF_SUPPORTED_SWING_MODES,
                default=list(SUPPORTED_SWING_MODES_OPTIONS.keys()),
            ): cv.ensure_list(cv.enum(SUPPORTED_SWING_MODES_OPTIONS, upper=True)),
            cv.Optional(
                CONF_SUPPORTED_FAN_MODES,
                default=list(SUPPORTED_FAN_MODES_OPTIONS.keys()),
            ): cv.ensure_list(cv.enum(SUPPORTED_FAN_MODES_OPTIONS, upper=True)),
            cv.Optional(
                SUPPORT_HVAC_ACTIONS,
                default=False,
            ): cv.boolean,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_visual,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)

    if CONF_SUPPORTED_MODES in config:
        cg.add(var.set_supported_climate_modes(config[CONF_SUPPORTED_MODES]))
    if CONF_SUPPORTED_SWING_MODES in config:
        cg.add(var.set_supported_swing_modes(config[CONF_SUPPORTED_SWING_MODES]))
    if CONF_SUPPORTED_FAN_MODES in config:
        cg.add(var.set_supported_fan_modes(config[CONF_SUPPORTED_FAN_MODES]))
    if SUPPORT_HVAC_ACTIONS in config:
        cg.add(var.set_support_hvac_actions(config[SUPPORT_HVAC_ACTIONS]))
