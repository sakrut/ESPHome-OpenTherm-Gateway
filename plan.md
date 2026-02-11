# Implementation Plan: Fix Temperature Display & External Sensor Support

## Issues Addressed

- **Issue #9** (Geminox THRi): Climate entity shows boiler water temperatures (51/61 C) instead of room temperatures (18.5/19 C)
- **Issue #10** (Feature Request): Support external HA sensor as current temperature source for climate entity

---

## Root Cause Analysis

### Issue #9: Why 51/61 Instead of 18.5/19

The `heating_water_climate` entity currently uses:
- `current_temperature = boiler_temperature` (Data-ID 25, Tboiler = ~51 C = water temp)
- `target_temperature = heating_target_temp` (Data-ID 1, TSet = ~61 C = water setpoint)

These are **boiler water** temperatures, not **room** temperatures. The user's thermostat (Siemens QAA73) sends room temperature (Data-ID 24, Tr = 18.5 C) and room setpoint (Data-ID 16, TrSet = 19 C) via OpenTherm WRITE-DATA messages, but our gateway ignores these values for climate entity display.

gduteil's project exposes `t_room` and `t_roomset` as separate sensors, which is why the user says "temperatures display correctly" there - those are dedicated room temperature sensors.

### Issue #10: External Sensor Need

Users want to use an external Home Assistant sensor (e.g., a Zigbee thermometer in another room) as the `current_temperature` source for the climate entity, instead of the boiler's built-in sensors.

### Relationship Between Issues

Both issues share the same root cause: the climate entity is designed around boiler water temperatures, but users expect to see room-level temperatures. The solution needs to support multiple temperature sources for the climate display.

---

## Research: How Other Projects Handle This

### arthurrump/esphome-opentherm (Official ESPHome OpenTherm)
- Acts as **thermostat replacement** (master only), not gateway
- Uses ESPHome's `platform: pid` climate with `sensor: <room_temp_id>` for room temperature display
- Room temperature imported via `platform: homeassistant` sensor with heartbeat filter
- `t_room` (Data-ID 24) is an **InputSchema** that writes room temp to boiler informationally
- PID controller translates room setpoint -> water setpoint

### gduteil/OpenThermGateway
- Gateway-style like ours
- Exposes `t_room` (Data-ID 24), `t_roomset` (Data-ID 16) as **separate read-only sensors**
- Also exposes `t_boiler`, `t_set`, etc. as separate sensors
- Uses **number platform** for setpoint overrides (`t_roomset_override`)
- No climate entity that conflates water and room temps

### KPWhiver/opentherm-gateway-esphome
- Gateway-style for Nodoshop hardware
- Accepts `outside_temperature` as a `platform: homeassistant` sensor reference injected into OT bus
- Uses ESPHome's built-in `platform: thermostat` climate with external room temp sensor

### jelenatko's Fork (User from Issue #9)
- Added `room_temperature` (Tr, Data-ID 24) and `room_setpoint` (TrSet, Data-ID 16) sensor caching
- Changed `heating_water_climate->current_temperature` from `boiler_temperature` to `room_temperature` with fallback
- Changed target_temperature from `heating_target_temp` (TSet) to `room_setpoint` (TrSet) with fallback
- Added equithermal curve calculation (but complex and fragile for a first implementation)
- Added startup protection / override detection logic

---

## Implementation Plan

### Phase 1: Room Temperature Sensors (Issue #9 Foundation)

**Goal**: Intercept and expose room temperature data from OpenTherm bus.

#### Step 1.1: Add Room Temperature & Setpoint Caching

**Files**: `opentherm_component.h`, `opentherm_component.cpp`

Add `CachedValue` entries for Tr (Data-ID 24) and TrSet (Data-ID 16):

In `opentherm_component.h`:
```cpp
CachedValue cached_room_temp_;       // Tr (Data-ID 24) - from thermostat WRITE
CachedValue cached_room_setpoint_;   // TrSet (Data-ID 16) - from thermostat WRITE
```

In `processCachedResponse()`:
```cpp
case OpenThermMessageID::Tr:
    cached_room_temp_.value = ot_->getFloat(response);
    cached_room_temp_.last_update = now;
    ESP_LOGV(TAG, "Cached room temp: %.1f C", cached_room_temp_.value);
    break;

case OpenThermMessageID::TrSet:
    cached_room_setpoint_.value = ot_->getFloat(response);
    cached_room_setpoint_.last_update = now;
    ESP_LOGV(TAG, "Cached room setpoint: %.1f C", cached_room_setpoint_.value);
    break;
```

**Critical**: Tr and TrSet are typically **WRITE-DATA from the thermostat** (not READ-DATA responses). The existing WRITE-DATA caching at `processRequest():404-412` already handles this. We just need the `processCachedResponse()` switch cases.

#### Step 1.2: Expose Room Temperature as Sensors

**Files**: `__init__.py`, `opentherm_component.h`, `opentherm_component.cpp`

In `__init__.py`:
```python
CONF_ROOM_TEMPERATURE = "room_temperature"
CONF_ROOM_SETPOINT = "room_setpoint"

# In CONFIG_SCHEMA:
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
```

In `opentherm_component.h`:
```cpp
sensor::Sensor *room_temperature_sensor_{nullptr};
sensor::Sensor *room_setpoint_sensor_{nullptr};

void set_room_temperature_sensor(sensor::Sensor *sensor) { room_temperature_sensor_ = sensor; }
void set_room_setpoint_sensor(sensor::Sensor *sensor) { room_setpoint_sensor_ = sensor; }
```

In `opentherm_component.cpp` `update()`:
```cpp
// Room temperature and setpoint (from thermostat WRITE-DATA)
float room_temperature = cached_room_temp_.value;
float room_setpoint = cached_room_setpoint_.value;

if (room_temperature_sensor_ != nullptr && !std::isnan(room_temperature))
    room_temperature_sensor_->publish_state(room_temperature);

if (room_setpoint_sensor_ != nullptr && !std::isnan(room_setpoint))
    room_setpoint_sensor_->publish_state(room_setpoint);
```

**Note**: Tr and TrSet are **write-only** from master to slave. We **cannot** use `getCachedOrFetch()` because the boiler doesn't respond to READ requests for these IDs. We rely entirely on intercepting thermostat WRITE-DATA messages. If the thermostat doesn't send them, the values will remain NAN.

#### Step 1.3: Update getRoomTemperature()

**Files**: `opentherm_component.cpp`

The current `getRoomTemperature()` tries to READ Data-ID 24 from the boiler, which will fail for most boilers (this is a master-to-slave write). Change to use cached value:

```cpp
float OpenthermComponent::getRoomTemperature() {
    // Tr (Data-ID 24) is WRITE-DATA from master to slave
    // We can only get it from intercepted thermostat communication
    return cached_room_temp_.value;
}
```

Add `getRoomSetpoint()`:
```cpp
float OpenthermComponent::getRoomSetpoint() {
    // TrSet (Data-ID 16) is WRITE-DATA from master to slave
    return cached_room_setpoint_.value;
}
```

---

### Phase 2: Climate Entity Temperature Source Configuration (Issues #9 + #10)

**Goal**: Allow the heating_water_climate entity to display room-level temperatures instead of water temperatures.

#### Step 2.1: Add `current_temperature_source` Config Option

**Files**: `__init__.py`, `opentherm_climate.h`, `opentherm_component.h`, `opentherm_component.cpp`

Add configuration option for the heating climate entity to select its temperature source:

In `opentherm_component.h`:
```cpp
enum class HeatingClimateMode {
    WATER,   // current_temp=Tboiler, target=TSet (current behavior, default)
    ROOM     // current_temp=Tr, target=TrSet (from thermostat)
};

HeatingClimateMode heating_climate_mode_{HeatingClimateMode::WATER};
void set_heating_climate_mode(HeatingClimateMode mode) { heating_climate_mode_ = mode; }
```

In `__init__.py`, add config option to `heating_water_climate`:
```python
CONF_TEMPERATURE_SOURCE = "temperature_source"

# In CONFIG_SCHEMA, modify heating_water_climate:
cv.Optional(CONF_HEATING_WATER_CLIMATE): climate.climate_schema(OpenthermClimate).extend({
    cv.Optional(CONF_TEMPERATURE_SOURCE, default="water"): cv.enum({
        "water": "WATER",
        "room": "ROOM",
    }),
}),
```

In `opentherm_component.cpp` `update()`:
```cpp
if (heating_water_climate_ != nullptr) {
    if (heating_climate_mode_ == HeatingClimateMode::ROOM) {
        // Room mode: show room temperature from thermostat
        float room_temp = cached_room_temp_.value;
        float room_setpoint = cached_room_setpoint_.value;
        heating_water_climate_->current_temperature =
            !std::isnan(room_temp) ? room_temp : boiler_temperature;
        heating_water_climate_->action = is_central_heating_active
            ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_OFF;
        float target = !std::isnan(room_setpoint) ? room_setpoint : heating_target_temp;
        heating_water_climate_->initialize_target_temperature(target);
    } else {
        // Water mode (default): show boiler water temperature
        heating_water_climate_->current_temperature = boiler_temperature;
        heating_water_climate_->action = is_central_heating_active
            ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_OFF;
        heating_water_climate_->initialize_target_temperature(heating_target_temp);
    }
    heating_water_climate_->publish_state();
}
```

**Important**: In ROOM mode, the climate entity target_temperature becomes the **room setpoint** (e.g., 19 C), but the callback still sends `TSet` (water setpoint) to the boiler. We need to handle the case where a user tries to set 21 C via the climate card, which should either:
- Be disabled (read-only display) in room mode, OR
- Write to `TrSet` (Data-ID 16) informing the thermostat, OR
- Convert to water temp via equithermal curve

**Recommended approach for Phase 2**: In ROOM mode, make the target temperature **read-only** (display only, follows thermostat). Users who want to override the setpoint should use the WATER mode or a separate number entity. This keeps the implementation simple and predictable.

#### Step 2.2: Adjust Climate Traits Based on Mode

When in ROOM mode, adjust the visual temperature range:

```cpp
// In OpenthermClimate::traits()
if (climate_type_ == ClimateType::HEATING_WATER && is_room_mode_) {
    traits.set_visual_min_temperature(5);
    traits.set_visual_max_temperature(35);  // Room temp range
} else {
    traits.set_visual_min_temperature(5);
    traits.set_visual_max_temperature(80);  // Water temp range
}
```

---

### Phase 3: External Home Assistant Sensor Support (Issue #10)

**Goal**: Accept an external Home Assistant sensor as the current_temperature source.

#### Step 3.1: Accept External Sensor Reference

This requires a `platform: homeassistant` sensor to be configured in the ESPHome YAML and passed to the component. The pattern:

```yaml
sensor:
  - platform: homeassistant
    id: room_temp_ha
    entity_id: sensor.living_room_temperature

opentherm:
  # ...
  heating_water_climate:
    name: "Central Heating"
    current_temperature_sensor: room_temp_ha
```

**Files**: `__init__.py`, `opentherm_component.h`, `opentherm_component.cpp`

In `opentherm_component.h`:
```cpp
sensor::Sensor *external_room_temp_sensor_{nullptr};

void set_external_room_temp_sensor(sensor::Sensor *sensor) {
    external_room_temp_sensor_ = sensor;
}
```

In `__init__.py`:
```python
CONF_CURRENT_TEMPERATURE_SENSOR = "current_temperature_sensor"

# In heating_water_climate schema:
cv.Optional(CONF_HEATING_WATER_CLIMATE): climate.climate_schema(OpenthermClimate).extend({
    cv.Optional(CONF_CURRENT_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
}),
```

In `opentherm_component.cpp` `update()`, priority for current_temperature:
1. External HA sensor (if configured and has valid value)
2. OpenTherm room temperature from thermostat (Tr, Data-ID 24, if available)
3. Boiler water temperature (Tboiler, Data-ID 25, fallback)

```cpp
if (heating_water_climate_ != nullptr) {
    float current_temp = boiler_temperature;  // Default fallback

    // Priority 1: External HA sensor
    if (external_room_temp_sensor_ != nullptr && external_room_temp_sensor_->has_state()) {
        current_temp = external_room_temp_sensor_->state;
    }
    // Priority 2: Room temp from thermostat (OT bus)
    else if (!std::isnan(cached_room_temp_.value)) {
        current_temp = cached_room_temp_.value;
    }

    heating_water_climate_->current_temperature = current_temp;
    // ...
}
```

---

### Phase 4: BLOR Fix (Issue #9, Secondary)

**Goal**: Fix the boiler reset validation logic.

The BLOR response `0x70040000` breaks down as:
- Msg type `0x7` = WRITE-ACK (binary: 0111) - boiler acknowledged the write
- Data-ID 4 = Command
- Data: `0x0000` = HB=0, LB=0

The current validation at `opentherm_component.cpp:590` checks:
```cpp
if (low_byte >= 128 || high_byte == 1)
```

A WRITE-ACK with data `0x0000` fails both conditions. But `ot_->isValidResponse()` should return `true` for WRITE-ACK. The issue is in the **secondary validation** after the valid response check.

**Fix**: A WRITE-ACK response to a BLOR command means the boiler accepted the command. The data value is irrelevant for BLOR:

```cpp
if (ot_->isValidResponse(response)) {
    OpenThermMessageType resp_type = ot_->getMessageType(response);
    if (resp_type == OpenThermMessageType::WRITE_ACK) {
        ESP_LOGI(TAG, "Boiler reset command acknowledged (WRITE-ACK)");
        return true;
    }
    // ... handle other response types
}
```

**Note**: Need to verify what `isValidResponse()` returns for WRITE-ACK. The OpenTherm library may only consider READ-ACK as valid. If so, we need to parse the message type directly from the response frame.

---

## Configuration Examples

### After Implementation - Issue #9 (Room Temperature Display)

```yaml
opentherm:
  id: opentherm_gateway
  in_pin: 4
  out_pin: 5
  slave_in_pin: 12
  slave_out_pin: 13

  # New: Room temperature sensors (from thermostat)
  room_temperature:
    name: "Room Temperature"
  room_setpoint:
    name: "Room Setpoint"

  # Existing: Water temperature sensors
  boiler_temperature:
    name: "Boiler Water Temperature"
  heating_target_temperature:
    name: "CH Water Setpoint"

  # Climate entity now shows room temperature
  heating_water_climate:
    name: "Central Heating"
    temperature_source: room    # NEW: "room" or "water" (default)
```

### After Implementation - Issue #10 (External Sensor)

```yaml
sensor:
  - platform: homeassistant
    id: room_temp_sensor
    entity_id: sensor.living_room_temperature

opentherm:
  id: opentherm_gateway
  in_pin: 4
  out_pin: 5
  slave_in_pin: 12
  slave_out_pin: 13

  heating_water_climate:
    name: "Central Heating"
    current_temperature_sensor: room_temp_sensor    # NEW
```

---

## Implementation Order

1. **Phase 1** (Steps 1.1-1.3): Room temperature sensor caching + exposure. Low risk, purely additive.
2. **Phase 4** (BLOR fix): Independent bug fix.
3. **Phase 2** (Steps 2.1-2.2): Climate entity temperature_source config. Medium complexity.
4. **Phase 3** (Step 3.1): External HA sensor support. Requires Phase 1+2.

## Testing Strategy

- **Python validation**: `python -m py_compile components/opentherm/__init__.py`
- **ESPHome config validation**: `esphome config build.yaml` (if available)
- **Full compile**: `esphome compile build.yaml` or Docker build
- **Manual testing**: Update `example_opentherm.yaml` with new sensor configs
- **Edge cases to test**:
  - Thermostat doesn't send Tr/TrSet (values stay NAN, fallback to water temps)
  - External HA sensor disconnects (fallback to OT room temp or water temp)
  - Startup before any data is received (NAN handling)

## Risk Assessment

- **Phase 1**: Low risk - purely additive sensors, no change to existing behavior
- **Phase 2**: Medium risk - changes climate entity behavior, needs default="water" for backward compatibility
- **Phase 3**: Medium risk - dependency on external sensor availability, needs robust NAN/fallback handling
- **Phase 4**: Low risk - isolated fix for response parsing

## jelenatko's Fork Analysis

jelenatko has done valuable exploratory work, but their implementation has some concerns:
- **Equithermal curve**: Hard-coded constants (CURVE_SLOPE=1.4, BASE_TEMP=25) are specific to their boiler
- **Startup protection**: 30-second ignore window and 20-cycle force-update are workarounds for HA state restoration, not the root cause
- **Override detection**: Complex tolerance-based comparison (0.5 C for DHW, 0.3 C for heating) is fragile

Our plan takes the clean parts (Tr/TrSet caching, fallback logic) and implements them in a simpler, more configurable way. The equithermal curve should be a separate optional feature if pursued at all.
