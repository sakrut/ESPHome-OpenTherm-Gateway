import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, climate, number
from esphome.const import (
    CONF_ID,
    CONF_INDEX,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_NUMBER,
    CONF_SENSOR,
    CONF_TEMPERATURE,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_HEAT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_PROBLEM,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    ENTITY_CATEGORY_DIAGNOSTIC,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_HECTOPASCAL,
)
from esphome import config_validation as cv
import esphome.core as core


CODEOWNERS = ["@sakrut"]
# The C++ sources always include these platform headers, so load them even when
# the user's YAML has no top-level sensor/binary_sensor/climate/button sections.
AUTO_LOAD = ["sensor", "binary_sensor", "climate", "button", "number"]
# Component constants
CONF_FLAME = "flame"
CONF_CH_ACTIVE = "ch_active"
CONF_DHW_ACTIVE = "dhw_active"
CONF_FAULT = "fault"
CONF_DIAGNOSTIC = "diagnostic"
CONF_EXTERNAL_TEMPERATURE = "external_temperature"
CONF_RETURN_TEMPERATURE = "return_temperature"
CONF_BOILER_TEMPERATURE = "boiler_temperature"
CONF_EXHAUST_TEMPERATURE = "exhaust_temperature"
CONF_PRESSURE = "pressure"
CONF_MODULATION = "modulation"
CONF_HEATING_TARGET_TEMPERATURE = "heating_target_temperature"
CONF_ROOM_TEMPERATURE = "room_temperature"
CONF_ROOM_SETPOINT = "room_setpoint"
CONF_HOT_WATER_CLIMATE = "hot_water_climate"
CONF_HEATING_WATER_CLIMATE = "heating_water_climate"
CONF_BLOCK_CH_WHEN_OFF = "block_ch_when_off"
CONF_ROOM_CLIMATE = "room_climate"
CONF_CURRENT_TEMPERATURE_SENSOR = "current_temperature_sensor"
CONF_READ_SENSORS = "read_sensors"
CONF_MESSAGE_ID = "message_id"
CONF_IN_PIN = "in_pin"
CONF_OUT_PIN = "out_pin"
CONF_SLAVE_IN_PIN = "slave_in_pin"
CONF_SLAVE_OUT_PIN = "slave_out_pin"
CONF_OPENTHERM_ID = "opentherm_id"
CONF_CLIMATE_TYPE = "climate_type"
# Phase 1 features
CONF_MAX_CH_SETPOINT = "max_ch_setpoint"
CONF_MIN_CH_SETPOINT = "min_ch_setpoint"
CONF_MAX_MODULATION = "max_modulation"
CONF_OEM_FAULT_CODE = "oem_fault_code"
CONF_OEM_DIAGNOSTIC_CODE = "oem_diagnostic_code"
CONF_MASTER_OT_VERSION = "master_ot_version"
CONF_SLAVE_OT_VERSION = "slave_ot_version"
CONF_DROPPED_FRAMES = "dropped_frames"
CONF_REJECTED_FRAMES = "rejected_frames"
CONF_FRAME_VALIDATION = "frame_validation"
CONF_CHECK_SPARE_BITS = "check_spare_bits"
CONF_CHECK_RESPONSE_ID = "check_response_id"
CONF_WRITE_CONFIRMATIONS = "write_confirmations"
CONF_TSP = "tsp"
CONF_SIGNED = "signed"
CONF_POLL_INTERVAL = "poll_interval"

# Generate namespaces
opentherm_ns = cg.esphome_ns.namespace("esphome::opentherm")
OpenthermComponent = opentherm_ns.class_("OpenthermComponent", cg.Component)
OpenthermClimate = opentherm_ns.class_("OpenthermClimate", climate.Climate, cg.Component)
OpenthermNumber = opentherm_ns.class_("OpenthermNumber", number.Number)
ClimateType = opentherm_ns.enum("ClimateType")


def validate_tsp_range(config):
    low, high = (-128, 127) if config[CONF_SIGNED] else (0, 255)
    if not low <= config[CONF_MIN_VALUE] <= config[CONF_MAX_VALUE] <= high:
        raise cv.Invalid(f"min_value..max_value must be ordered and within {low}..{high}")
    return config


TSP_SCHEMA = cv.All(
    cv.Schema({
        cv.Required(CONF_INDEX): cv.hex_uint8_t,
        cv.Optional(CONF_SIGNED, default=False): cv.boolean,
        cv.Required(CONF_MIN_VALUE): cv.int_,
        cv.Required(CONF_MAX_VALUE): cv.int_,
        cv.Optional(CONF_POLL_INTERVAL, default="5min"): cv.All(
            cv.positive_time_period_milliseconds, cv.Range(min=cv.TimePeriod(minutes=1))
        ),
        cv.Optional(CONF_SENSOR): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_NUMBER): number.number_schema(OpenthermNumber),
    }),
    validate_tsp_range,
)

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
    cv.Optional(CONF_UPDATE_INTERVAL, default="30s"): cv.positive_time_period_milliseconds,
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
    cv.Optional(CONF_EXHAUST_TEMPERATURE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=0,
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
    cv.Optional(CONF_ROOM_TEMPERATURE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_ROOM_SETPOINT): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_READ_SENSORS): cv.ensure_list(
        sensor.sensor_schema(
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend({
            cv.Required(CONF_MESSAGE_ID): cv.int_range(min=0, max=255),
        })
    ),
    # Phase 1 sensors - Boiler limits and diagnostics
    cv.Optional(CONF_MAX_CH_SETPOINT): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_MIN_CH_SETPOINT): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_MAX_MODULATION): sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_OEM_FAULT_CODE): sensor.sensor_schema(
        accuracy_decimals=0,
    ),
    cv.Optional(CONF_OEM_DIAGNOSTIC_CODE): sensor.sensor_schema(
        accuracy_decimals=0,
    ),
    cv.Optional(CONF_MASTER_OT_VERSION): sensor.sensor_schema(
        accuracy_decimals=2,
    ),
    cv.Optional(CONF_SLAVE_OT_VERSION): sensor.sensor_schema(
        accuracy_decimals=2,
    ),
    cv.Optional(CONF_DROPPED_FRAMES): sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:alert-circle-outline",
    ),
    cv.Optional(CONF_REJECTED_FRAMES): sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:shield-alert-outline",
    ),
    cv.Optional(CONF_FRAME_VALIDATION, default={}): cv.Schema({
        cv.Optional(CONF_CHECK_SPARE_BITS, default=True): cv.boolean,
        cv.Optional(CONF_CHECK_RESPONSE_ID, default=True): cv.boolean,
        cv.Optional(CONF_WRITE_CONFIRMATIONS, default=2): cv.int_range(min=1, max=5),
    }),
    cv.Optional(CONF_TSP): cv.ensure_list(TSP_SCHEMA),
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
    cv.Optional(CONF_HOT_WATER_CLIMATE): climate.climate_schema(
        OpenthermClimate,
    ),
    cv.Optional(CONF_HEATING_WATER_CLIMATE): climate.climate_schema(
        OpenthermClimate,
    ).extend({
        # Off was a no-op before; existing automations turning it off must not start blocking CH
        cv.Optional(CONF_BLOCK_CH_WHEN_OFF, default=False): cv.boolean,
    }),
    cv.Optional(CONF_ROOM_CLIMATE): climate.climate_schema(
        OpenthermClimate,
    ).extend({
        cv.Optional(CONF_CURRENT_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
    }),
}).extend(cv.COMPONENT_SCHEMA)



async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_UPDATE_INTERVAL])
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

    if CONF_EXHAUST_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_EXHAUST_TEMPERATURE])
        cg.add(var.set_exhaust_temperature_sensor(sens))
    
    if CONF_PRESSURE in config:
        sens = await sensor.new_sensor(config[CONF_PRESSURE])
        cg.add(var.set_pressure_sensor(sens))
    
    if CONF_MODULATION in config:
        sens = await sensor.new_sensor(config[CONF_MODULATION])
        cg.add(var.set_modulation_sensor(sens))
    
    if CONF_HEATING_TARGET_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_HEATING_TARGET_TEMPERATURE])
        cg.add(var.set_heating_target_temperature_sensor(sens))
    
    if CONF_ROOM_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_ROOM_TEMPERATURE])
        cg.add(var.set_room_temperature_sensor(sens))

    if CONF_ROOM_SETPOINT in config:
        sens = await sensor.new_sensor(config[CONF_ROOM_SETPOINT])
        cg.add(var.set_room_setpoint_sensor(sens))

    for read_sensor_conf in config.get(CONF_READ_SENSORS, []):
        sens = await sensor.new_sensor(read_sensor_conf)
        cg.add(var.add_read_sensor(sens, read_sensor_conf[CONF_MESSAGE_ID]))

    # Phase 1 sensors
    if CONF_MAX_CH_SETPOINT in config:
        sens = await sensor.new_sensor(config[CONF_MAX_CH_SETPOINT])
        cg.add(var.set_max_ch_setpoint_sensor(sens))

    if CONF_MIN_CH_SETPOINT in config:
        sens = await sensor.new_sensor(config[CONF_MIN_CH_SETPOINT])
        cg.add(var.set_min_ch_setpoint_sensor(sens))

    if CONF_MAX_MODULATION in config:
        sens = await sensor.new_sensor(config[CONF_MAX_MODULATION])
        cg.add(var.set_max_modulation_sensor(sens))

    if CONF_OEM_FAULT_CODE in config:
        sens = await sensor.new_sensor(config[CONF_OEM_FAULT_CODE])
        cg.add(var.set_oem_fault_code_sensor(sens))

    if CONF_OEM_DIAGNOSTIC_CODE in config:
        sens = await sensor.new_sensor(config[CONF_OEM_DIAGNOSTIC_CODE])
        cg.add(var.set_oem_diagnostic_code_sensor(sens))

    if CONF_MASTER_OT_VERSION in config:
        sens = await sensor.new_sensor(config[CONF_MASTER_OT_VERSION])
        cg.add(var.set_master_ot_version_sensor(sens))

    if CONF_SLAVE_OT_VERSION in config:
        sens = await sensor.new_sensor(config[CONF_SLAVE_OT_VERSION])
        cg.add(var.set_slave_ot_version_sensor(sens))

    if CONF_DROPPED_FRAMES in config:
        sens = await sensor.new_sensor(config[CONF_DROPPED_FRAMES])
        cg.add(var.set_dropped_frames_sensor(sens))

    if CONF_REJECTED_FRAMES in config:
        sens = await sensor.new_sensor(config[CONF_REJECTED_FRAMES])
        cg.add(var.set_rejected_frames_sensor(sens))

    validation = config[CONF_FRAME_VALIDATION]
    cg.add(var.set_check_spare_bits(validation[CONF_CHECK_SPARE_BITS]))
    cg.add(var.set_check_response_id(validation[CONF_CHECK_RESPONSE_ID]))
    cg.add(var.set_write_confirmations(validation[CONF_WRITE_CONFIRMATIONS]))

    for tsp in config.get(CONF_TSP, []):
        sens = await sensor.new_sensor(tsp[CONF_SENSOR]) if CONF_SENSOR in tsp else cg.nullptr
        num = (
            await number.new_number(
                tsp[CONF_NUMBER],
                min_value=tsp[CONF_MIN_VALUE],
                max_value=tsp[CONF_MAX_VALUE],
                step=1,
            )
            if CONF_NUMBER in tsp
            else cg.nullptr
        )
        cg.add(var.add_tsp(
            tsp[CONF_INDEX],
            tsp[CONF_SIGNED],
            tsp[CONF_MIN_VALUE],
            tsp[CONF_MAX_VALUE],
            tsp[CONF_POLL_INTERVAL],
            sens,
            num,
        ))

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
        cg.add(var.set_block_ch_when_off(heating_conf[CONF_BLOCK_CH_WHEN_OFF]))
        cg.add(heating_var.set_persist_mode(heating_conf[CONF_BLOCK_CH_WHEN_OFF]))

    if CONF_ROOM_CLIMATE in config:
        room_conf = config[CONF_ROOM_CLIMATE]
        room_var = cg.new_Pvariable(room_conf[CONF_ID])
        await cg.register_component(room_var, room_conf)
        await climate.register_climate(room_var, room_conf)
        cg.add(room_var.set_climate_type(ClimateType.ROOM))
        cg.add(room_var.set_read_only(True))
        cg.add(var.register_climate(room_var))
        if CONF_CURRENT_TEMPERATURE_SENSOR in room_conf:
            ext_sensor = await cg.get_variable(room_conf[CONF_CURRENT_TEMPERATURE_SENSOR])
            cg.add(var.set_room_climate_external_sensor(ext_sensor))
    
    # Add library dependencies
    cg.add_library("ihormelnyk/OpenTherm Library", "1.1.4")
