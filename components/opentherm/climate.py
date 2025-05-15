import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.const import (
    CONF_ID,
    CONF_MODE,
    CONF_TARGET_TEMPERATURE,
    CONF_TARGET_TEMPERATURE_HIGH,
    CONF_TARGET_TEMPERATURE_LOW,
    CONF_TEMPERATURE,
)
from . import opentherm_ns, OpenthermComponent, OpenthermClimate

CONF_OPENTHERM_ID = "opentherm_id"
CONF_CLIMATE_TYPE = "climate_type"

ClimateType = opentherm_ns.enum("ClimateType")
CLIMATE_TYPES = {
    "hot_water": opentherm_ns.namespace.HOT_WATER,
    "heating_water":  opentherm_ns.namespace.HEATING_WATER
}

CONFIG_SCHEMA = climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(OpenthermClimate),
    cv.GenerateID(CONF_OPENTHERM_ID): cv.use_id(OpenthermComponent),
    cv.Required(CONF_CLIMATE_TYPE): cv.enum(CLIMATE_TYPES, lower=True),
})

async def to_code(config):
    parent = await cg.get_variable(config[CONF_OPENTHERM_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await climate.register_climate(var, config)
    
    cg.add(var.set_climate_type(config[CONF_CLIMATE_TYPE]))
    cg.add(parent.register_climate(var))