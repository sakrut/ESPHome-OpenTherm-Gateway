import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_HEAT,
    DEVICE_CLASS_PROBLEM,
)
from . import opentherm_ns, OpenthermComponent, CONF_ID

DEPENDENCIES = ["opentherm"]
CODEOWNERS = ["@yourusername"]

# Component constants
CONF_FLAME = "flame"
CONF_CH_ACTIVE = "ch_active"
CONF_DHW_ACTIVE = "dhw_active"
CONF_FAULT = "fault"
CONF_DIAGNOSTIC = "diagnostic"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ID): cv.use_id(OpenthermComponent),
    cv.Optional(CONF_FLAME): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_HEAT,
    ),
    cv.Optional(CONF_CH_ACTIVE): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_HEAT,
    ),
    cv.Optional(CONF_DHW_ACTIVE): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_HEAT,
    ),
    cv.Optional(CONF_FAULT): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PROBLEM,
    ),
    cv.Optional(CONF_DIAGNOSTIC): binary_sensor.binary_sensor_schema(),
})

async def to_code(config):
    hub = await cg.get_variable(config[CONF_ID])
    
    # Register binary sensors
    if CONF_FLAME in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_FLAME])
        cg.add(hub.set_flame_sensor(sens))
    
    if CONF_CH_ACTIVE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_CH_ACTIVE])
        cg.add(hub.set_ch_active_sensor(sens))
    
    if CONF_DHW_ACTIVE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_DHW_ACTIVE])
        cg.add(hub.set_dhw_active_sensor(sens))
    
    if CONF_FAULT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_FAULT])
        cg.add(hub.set_fault_sensor(sens))
    
    if CONF_DIAGNOSTIC in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_DIAGNOSTIC])
        cg.add(hub.set_diagnostic_sensor(sens))