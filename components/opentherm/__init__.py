import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, climate
from esphome.const import (
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_HEAT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_PROBLEM,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_HECTOPASCAL,
)


CODEOWNERS = ["@sakrut"]
DEPENDENCIES = ["binary_sensor", "sensor", "climate"]
REQUIRED_DEPENDENCIES = ["binary_sensor", "sensor", "climate"]
# Component constants
CONF_FLAME = "flame"
CONF_CH_ACTIVE = "ch_active"
CONF_DHW_ACTIVE = "dhw_active"
CONF_FAULT = "fault"
CONF_DIAGNOSTIC = "diagnostic"
CONF_EXTERNAL_TEMPERATURE = "external_temperature"
CONF_RETURN_TEMPERATURE = "return_temperature"
CONF_BOILER_TEMPERATURE = "boiler_temperature"
CONF_PRESSURE = "pressure"
CONF_MODULATION = "modulation"
CONF_HEATING_TARGET_TEMPERATURE = "heating_target_temperature"
CONF_HOT_WATER_CLIMATE = "hot_water_climate"
CONF_HEATING_WATER_CLIMATE = "heating_water_climate"
CONF_IN_PIN = "in_pin"
CONF_OUT_PIN = "out_pin"
CONF_SLAVE_IN_PIN = "slave_in_pin"
CONF_SLAVE_OUT_PIN = "slave_out_pin"
CONF_OPENTHERM_ID = "opentherm_id"
CONF_CLIMATE_TYPE = "climate_type"

# Generate namespaces
opentherm_ns = cg.esphome_ns.namespace("esphome::opentherm")
OpenthermComponent = opentherm_ns.class_("OpenthermComponent", cg.Component)
OpenthermClimate = opentherm_ns.class_("OpenthermClimate", climate.Climate, cg.Component)
ClimateType = opentherm_ns.enum("ClimateType")

# Climate types mapping
CLIMATE_TYPES = {
    "hot_water": ClimateType.HOT_WATER,
    "heating_water": ClimateType.HEATING_WATER,
}

# Validation schema
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(OpenthermComponent),
    cv.Required(CONF_IN_PIN): cv.int_,
    cv.Required(CONF_OUT_PIN): cv.int_,
    cv.Required(CONF_SLAVE_IN_PIN): cv.int_,
    cv.Required(CONF_SLAVE_OUT_PIN): cv.int_,
    cv.Optional(CONF_EXTERNAL_TEMPERATURE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_RETURN_TEMPERATURE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_BOILER_TEMPERATURE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
        unit_of_measurement=UNIT_HECTOPASCAL,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_PRESSURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_MODULATION): sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_HEATING_TARGET_TEMPERATURE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
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
    cv.Optional(CONF_HOT_WATER_CLIMATE): climate.CLIMATE_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(OpenthermClimate),
    }),
    cv.Optional(CONF_HEATING_WATER_CLIMATE): climate.CLIMATE_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(OpenthermClimate),
    }),
}).extend(cv.COMPONENT_SCHEMA)



async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # Add pins to component
    cg.add(var.set_in_pin(config[CONF_IN_PIN]))
    cg.add(var.set_out_pin(config[CONF_OUT_PIN]))
    cg.add(var.set_slave_in_pin(config[CONF_SLAVE_IN_PIN]))
    cg.add(var.set_slave_out_pin(config[CONF_SLAVE_OUT_PIN]))

    # Register sensors
    if CONF_EXTERNAL_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_EXTERNAL_TEMPERATURE])
        cg.add(var.set_external_temperature_sensor(sens))
    
    if CONF_RETURN_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_RETURN_TEMPERATURE])
        cg.add(var.set_return_temperature_sensor(sens))
    
    if CONF_BOILER_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_BOILER_TEMPERATURE])
        cg.add(var.set_boiler_temperature_sensor(sens))
    
    if CONF_PRESSURE in config:
        sens = await sensor.new_sensor(config[CONF_PRESSURE])
        cg.add(var.set_pressure_sensor(sens))
    
    if CONF_MODULATION in config:
        sens = await sensor.new_sensor(config[CONF_MODULATION])
        cg.add(var.set_modulation_sensor(sens))
    
    if CONF_HEATING_TARGET_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_HEATING_TARGET_TEMPERATURE])
        cg.add(var.set_heating_target_temperature_sensor(sens))
    
    # Register binary sensors
    if CONF_FLAME in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_FLAME])
        cg.add(var.set_flame_sensor(sens))
    
    if CONF_CH_ACTIVE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_CH_ACTIVE])
        cg.add(var.set_ch_active_sensor(sens))
    
    if CONF_DHW_ACTIVE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_DHW_ACTIVE])
        cg.add(var.set_dhw_active_sensor(sens))
    
    if CONF_FAULT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_FAULT])
        cg.add(var.set_fault_sensor(sens))
    
    if CONF_DIAGNOSTIC in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_DIAGNOSTIC])
        cg.add(var.set_diagnostic_sensor(sens))

    # Register climate controllers if defined
    if CONF_HOT_WATER_CLIMATE in config:
        hot_water_conf = config[CONF_HOT_WATER_CLIMATE]
        hot_water_var = cg.new_Pvariable(hot_water_conf[CONF_ID])
        await cg.register_component(hot_water_var, hot_water_conf)
        await climate.register_climate(hot_water_var, hot_water_conf)
        cg.add(hot_water_var.set_climate_type(ClimateType.HOT_WATER))
        cg.add(var.register_climate(hot_water_var))

    if CONF_HEATING_WATER_CLIMATE in config:
        heating_conf = config[CONF_HEATING_WATER_CLIMATE]
        heating_var = cg.new_Pvariable(heating_conf[CONF_ID])
        await cg.register_component(heating_var, heating_conf)
        await climate.register_climate(heating_var, heating_conf)
        cg.add(heating_var.set_climate_type(ClimateType.HEATING_WATER))
        cg.add(var.register_climate(heating_var))
    
    # Add library dependencies
    cg.add_library("ihormelnyk/OpenTherm Library", "1.1.4")