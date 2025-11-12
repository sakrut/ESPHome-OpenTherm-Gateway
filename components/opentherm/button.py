import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID, ICON_RESTART
from . import opentherm_ns, OpenthermComponent

CONF_OPENTHERM_ID = "opentherm_id"

DEPENDENCIES = ["opentherm"]
CODEOWNERS = ["@sakrut"]

OpenthermResetButton = opentherm_ns.class_(
    "OpenthermResetButton", button.Button, cg.Component
)

CONFIG_SCHEMA = button.button_schema(
    OpenthermResetButton,
    icon=ICON_RESTART,
).extend(
    {
        cv.GenerateID(CONF_OPENTHERM_ID): cv.use_id(OpenthermComponent),
    }
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_OPENTHERM_ID])
    cg.add(var.set_parent(parent))
