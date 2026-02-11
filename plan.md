# Implementation Plan: Room Temperature Climate Entity & External Sensor Support

## Issues Addressed

- **Issue #9** (Geminox THRi): User sees boiler water temperatures (51/61 C) but expects room temperatures (18.5/19 C)
- **Issue #10** (Feature Request): Support external HA sensor as current temperature source

---

## Root Cause Analysis

### Issue #9: Why 51/61 Instead of 18.5/19

The user's confusion stems from the `heating_water_climate` entity showing boiler water temperatures:
- `current_temperature` = Tboiler (Data-ID 25) = ~51 C (water leaving boiler)
- `target_temperature` = TSet (Data-ID 1) = ~61 C (water setpoint)

These are **correct** for a boiler water control entity. The problem is there's **no separate entity** showing room-level temperatures. The thermostat (Siemens QAA73) sends room temperature (Tr, Data-ID 24 = 18.5 C) and room setpoint (TrSet, Data-ID 16 = 19 C) via OpenTherm WRITE-DATA messages, but the gateway doesn't expose them.

gduteil's project exposes `t_room` and `t_roomset` as separate sensors, which is why the user says "temperatures display correctly" there.

### Issue #10: External Sensor Need

Users want to use a Home Assistant sensor (Zigbee thermometer, Dallas probe, etc.) as the displayed room temperature, not the boiler's internal sensors.

### Architectural Principle: We Are a Gateway, Not a Thermostat

This component is a **man-in-the-middle gateway**. The original thermostat remains in control. We should:
- **Observe** and expose data flowing between thermostat and boiler
- **NOT** try to replace the thermostat's heating logic
- Keep `heating_water_climate` as boiler water temperature control (for users who want to override water setpoint)
- Add a **separate** `room_climate` entity for room-level temperature display

This aligns with how proven gateway projects handle it:
- **gduteil/OpenThermGateway**: Separate read-only sensors for room temp / room setpoint
- **KPWhiver/opentherm-gateway-esphome**: Separate `platform: thermostat` climate with external room temp sensor
- Neither gateway project mixes water and room temps in one entity

---

## Research: How Other Projects Handle This

### arthurrump/esphome-opentherm (Official ESPHome OpenTherm)
- Acts as **thermostat replacement** (master only), not gateway
- Uses ESPHome's `platform: pid` climate with `sensor:` for room temperature
- PID controller translates room setpoint -> water setpoint
- Not applicable to our gateway architecture

### gduteil/OpenThermGateway (Gateway, like us)
- Exposes `t_room` (Data-ID 24), `t_roomset` (Data-ID 16) as **separate read-only sensors**
- Also exposes `t_boiler`, `t_set` as separate sensors
- Uses **number platform** for setpoint overrides (`t_roomset_override`)
- No climate entity that conflates water and room temps
- **Key insight**: Clean separation between water-level and room-level data

### KPWhiver/opentherm-gateway-esphome (Gateway)
- Accepts `outside_temperature` as `platform: homeassistant` sensor reference
- Uses ESPHome's built-in `platform: thermostat` climate with external room temp sensor
- Separate climate entity for room-level control
- **Key insight**: External sensor pattern via `cv.use_id(sensor.Sensor)`

### jelenatko's Fork (User from Issue #9)
- Cached Tr (Data-ID 24) and TrSet (Data-ID 16) from thermostat WRITE-DATA (**good, we adopt this**)
- Changed `heating_water_climate` to show room temps instead of water temps (**we disagree - separate entity is cleaner**)
- Added equithermal curve to convert room setpoint -> water setpoint (**over-engineering for gateway**)
- Added startup protection / override detection (**workarounds, not root cause fixes**)

---

## Implementation Plan

### Phase 1: Room Temperature Sensors (Foundation)

**Goal**: Intercept and expose room temperature data flowing on the OpenTherm bus.

#### Step 1.1: Add Room Temperature & Setpoint Caching

**Files**: `opentherm_component.h`, `opentherm_component.cpp`

Add `CachedValue` entries for Tr (Data-ID 24) and TrSet (Data-ID 16):

In `opentherm_component.h`:
```cpp
CachedValue cached_room_temp_;       // Tr (Data-ID 24) - from thermostat WRITE
CachedValue cached_room_setpoint_;   // TrSet (Data-ID 16) - from thermostat WRITE
```

In `processCachedResponse()` add switch cases:
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

**Why this works**: Tr and TrSet are **WRITE-DATA from the thermostat** (master -> slave).
The existing WRITE-DATA caching at `processRequest():404-412` already stores these.
We just need `processCachedResponse()` switch cases to extract the float values.

**Important**: We **cannot** use `getCachedOrFetch()` for these because the boiler doesn't
respond to READ requests for Data-IDs 24/16. We rely entirely on intercepting thermostat
WRITE-DATA messages. If the thermostat doesn't send them, values remain NAN.

#### Step 1.2: Expose Room Temperature as Hub Sensors

**Files**: `__init__.py`, `opentherm_component.h`, `opentherm_component.cpp`

In `__init__.py`:
```python
CONF_ROOM_TEMPERATURE = "room_temperature"
CONF_ROOM_SETPOINT = "room_setpoint"

# Add to CONFIG_SCHEMA:
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

# Add to to_code():
if CONF_ROOM_TEMPERATURE in config:
    sens = await sensor.new_sensor(config[CONF_ROOM_TEMPERATURE])
    cg.add(var.set_room_temperature_sensor(sens))

if CONF_ROOM_SETPOINT in config:
    sens = await sensor.new_sensor(config[CONF_ROOM_SETPOINT])
    cg.add(var.set_room_setpoint_sensor(sens))
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
// Room temperature and setpoint (from thermostat WRITE-DATA, no active polling)
float room_temperature = cached_room_temp_.value;
float room_setpoint = cached_room_setpoint_.value;

if (room_temperature_sensor_ != nullptr && !std::isnan(room_temperature))
    room_temperature_sensor_->publish_state(room_temperature);

if (room_setpoint_sensor_ != nullptr && !std::isnan(room_setpoint))
    room_setpoint_sensor_->publish_state(room_setpoint);
```

#### Step 1.3: Fix getRoomTemperature()

**Files**: `opentherm_component.h`, `opentherm_component.cpp`

Current `getRoomTemperature()` tries to READ Data-ID 24 from boiler, which fails
(Tr is master-to-slave WRITE, not readable). Change to use cached value:

```cpp
float OpenthermComponent::getRoomTemperature() {
    // Tr (Data-ID 24) is WRITE-DATA from master (thermostat) to slave (boiler)
    // We intercept it from the thermostat communication, cannot read it from boiler
    return cached_room_temp_.value;
}
```

Add `getRoomSetpoint()` declaration to header and implementation:
```cpp
float OpenthermComponent::getRoomSetpoint() {
    // TrSet (Data-ID 16) is WRITE-DATA from master to slave
    return cached_room_setpoint_.value;
}
```

---

### Phase 2: Room Climate Entity (Separate, Read-Only)

**Goal**: Add a new `room_climate` entity that displays room-level temperatures.
The existing `heating_water_climate` stays **unchanged** (boiler water control).

#### Architecture

| Entity | current_temp | target_temp | Controllable? | Purpose |
|--------|-------------|-------------|---------------|---------|
| `heating_water_climate` (existing) | Tboiler (25) | TSet (1) | Yes - WRITE to boiler | Boiler water temp control |
| `hot_water_climate` (existing) | Tdhw (26) | TdhwSet (56) | Yes - WRITE to boiler | DHW water temp control |
| **`room_climate` (NEW)** | Tr (24) or external | TrSet (16) | **No - read-only display** | Room temperature monitoring |

The room_climate is **informational only**. We're a gateway - the thermostat controls
room temperature. We just observe and display what's happening.

This follows the pattern from KPWhiver's gateway which uses a separate climate entity
for room temperature, and gduteil's gateway which exposes room temp as separate sensors.

#### Step 2.1: Add ClimateType::ROOM

**Files**: `opentherm_component.h`

```cpp
enum class ClimateType {
    HOT_WATER,
    HEATING_WATER,
    ROOM              // NEW
};
```

#### Step 2.2: Make OpenthermClimate Support Read-Only Mode

**Files**: `opentherm_climate.h`, `opentherm_climate.cpp`

In `opentherm_climate.h`, add read-only flag:
```cpp
bool read_only_{false};

void set_read_only(bool read_only) { read_only_ = read_only; }
```

In `opentherm_climate.cpp`, modify `traits()` for room climate:
```cpp
climate::ClimateTraits OpenthermClimate::traits() {
    auto traits = climate::ClimateTraits();
    traits.set_supports_current_temperature(true);
    traits.set_supports_action(true);

    if (read_only_) {
        // Room climate: read-only, room temperature range
        traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT});
        traits.set_supports_two_point_target_temperature(false);
        traits.set_visual_min_temperature(5);
        traits.set_visual_max_temperature(35);
        traits.set_visual_temperature_step(0.5);
    } else {
        // Water climate: controllable, water temperature range
        traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT});
        traits.set_supports_two_point_target_temperature(false);
        traits.set_visual_min_temperature(5);
        traits.set_visual_max_temperature(80);
        traits.set_visual_temperature_step(1);
    }

    return traits;
}
```

In `control()`, ignore target temperature changes for read-only climate:
```cpp
void OpenthermClimate::control(const climate::ClimateCall &call) {
    if (call.get_mode().has_value()) {
        this->mode = *call.get_mode();
        this->publish_state();
    }

    if (call.get_target_temperature().has_value() && !read_only_) {
        float temp = *call.get_target_temperature();
        this->target_temperature = temp;
        target_temperature_initialized_ = true;
        if (target_temperature_setter_) {
            target_temperature_setter_(temp);
        }
        this->publish_state();
    }
}
```

#### Step 2.3: Register Room Climate in Config

**Files**: `__init__.py`, `opentherm_component.h`, `opentherm_component.cpp`

In `__init__.py`:
```python
CONF_ROOM_CLIMATE = "room_climate"

# Add to CONFIG_SCHEMA:
cv.Optional(CONF_ROOM_CLIMATE): climate.climate_schema(OpenthermClimate),

# Add to to_code():
if CONF_ROOM_CLIMATE in config:
    room_conf = config[CONF_ROOM_CLIMATE]
    room_var = cg.new_Pvariable(room_conf[CONF_ID])
    await cg.register_component(room_var, room_conf)
    await climate.register_climate(room_var, room_conf)
    cg.add(room_var.set_climate_type(ClimateType.ROOM))
    cg.add(room_var.set_read_only(True))
    cg.add(var.register_climate(room_var))
```

In `opentherm_component.h`:
```cpp
OpenthermClimate *room_climate_{nullptr};

// Update register_climate to handle ROOM type
```

In `opentherm_component.cpp`, update `register_climate()`:
```cpp
void OpenthermComponent::register_climate(OpenthermClimate *climate) {
    ClimateType type = climate->get_climate_type();
    if (type == ClimateType::HOT_WATER) {
        hot_water_climate_ = climate;
    } else if (type == ClimateType::HEATING_WATER) {
        heating_water_climate_ = climate;
    } else if (type == ClimateType::ROOM) {
        room_climate_ = climate;
    }
}
```

#### Step 2.4: Update Room Climate in update()

In `opentherm_component.cpp` `update()`, add after existing climate updates:
```cpp
// Room climate (read-only, displays thermostat values)
if (room_climate_ != nullptr) {
    float room_temp = cached_room_temp_.value;
    float room_setpoint = cached_room_setpoint_.value;

    if (!std::isnan(room_temp))
        room_climate_->current_temperature = room_temp;

    room_climate_->action = is_central_heating_active
        ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_OFF;

    // Update target from thermostat setpoint (read-only, follows thermostat)
    if (!std::isnan(room_setpoint)) {
        room_climate_->target_temperature = room_setpoint;
    }

    room_climate_->publish_state();
}
```

Note: `heating_water_climate` code remains **completely unchanged**.

---

### Phase 3: External HA Sensor for Room Climate (Issue #10)

**Goal**: Accept an external Home Assistant sensor as `current_temperature` source
for the `room_climate` entity. This is for users whose thermostat doesn't send Tr
(Data-ID 24), or who want to use a different temperature source.

This follows the pattern from KPWhiver's gateway:
```yaml
sensor:
  - platform: homeassistant
    id: room_temp_ha
    entity_id: sensor.living_room_temperature
```

#### Step 3.1: Add External Sensor Config to Room Climate

**Files**: `__init__.py`, `opentherm_component.h`, `opentherm_component.cpp`

In `__init__.py`, extend room_climate schema:
```python
CONF_CURRENT_TEMPERATURE_SENSOR = "current_temperature_sensor"

cv.Optional(CONF_ROOM_CLIMATE): climate.climate_schema(OpenthermClimate).extend({
    cv.Optional(CONF_CURRENT_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
}),

# In to_code(), after registering room climate:
if CONF_CURRENT_TEMPERATURE_SENSOR in room_conf:
    ext_sensor = await cg.get_variable(room_conf[CONF_CURRENT_TEMPERATURE_SENSOR])
    cg.add(var.set_room_climate_external_sensor(ext_sensor))
```

In `opentherm_component.h`:
```cpp
sensor::Sensor *room_climate_external_sensor_{nullptr};

void set_room_climate_external_sensor(sensor::Sensor *sensor) {
    room_climate_external_sensor_ = sensor;
}
```

#### Step 3.2: Temperature Source Priority in update()

In `opentherm_component.cpp`, update the room_climate section:
```cpp
if (room_climate_ != nullptr) {
    float room_temp = NAN;

    // Priority 1: External HA sensor (if configured and has valid state)
    if (room_climate_external_sensor_ != nullptr && room_climate_external_sensor_->has_state()) {
        room_temp = room_climate_external_sensor_->state;
    }
    // Priority 2: Room temp from thermostat (Tr, Data-ID 24, intercepted from OT bus)
    else if (!std::isnan(cached_room_temp_.value)) {
        room_temp = cached_room_temp_.value;
    }

    if (!std::isnan(room_temp))
        room_climate_->current_temperature = room_temp;

    // Target temperature always from thermostat (TrSet, Data-ID 16)
    float room_setpoint = cached_room_setpoint_.value;
    if (!std::isnan(room_setpoint)) {
        room_climate_->target_temperature = room_setpoint;
    }

    room_climate_->action = is_central_heating_active
        ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_OFF;
    room_climate_->publish_state();
}
```

---

### Phase 4: BLOR Fix (Issue #9, Secondary Bug)

**Goal**: Fix the boiler reset response validation.

The BLOR response `0x70040000` decodes as:
- Msg type bits `0111` = WRITE-ACK (boiler acknowledged the command)
- Data-ID 4 = Command
- Data `0x0000` = no additional data

Current validation at `opentherm_component.cpp:590`:
```cpp
if (low_byte >= 128 || high_byte == 1)  // Fails for 0x0000!
```

**Fix**: A WRITE-ACK means the boiler accepted the command. Check response type, not data:
```cpp
if (ot_->isValidResponse(response)) {
    OpenThermMessageType resp_type = ot_->getMessageType(response);

    uint16_t response_data = response & 0xFFFF;
    uint8_t high_byte = (response_data >> 8) & 0xFF;
    uint8_t low_byte = response_data & 0xFF;
    ESP_LOGD(TAG, "BLOR response data: HB=0x%02X, LB=0x%02X, type=%d",
             high_byte, low_byte, static_cast<int>(resp_type));

    if (resp_type == OpenThermMessageType::WRITE_ACK) {
        ESP_LOGI(TAG, "Boiler reset command acknowledged (WRITE-ACK)");
        return true;
    } else {
        ESP_LOGW(TAG, "BLOR unexpected response type: %d", static_cast<int>(resp_type));
        return false;
    }
}
```

**Note**: Need to verify `ot_->isValidResponse()` returns true for WRITE-ACK.
The OpenTherm library defines valid responses as both READ-ACK and WRITE-ACK in
the `isValidResponse()` method. If not, we need to check message type from raw frame.

---

## Configuration Examples

### Full Example: All Three Climate Entities + Room Sensors

```yaml
sensor:
  # Optional: External room temperature from Home Assistant
  - platform: homeassistant
    id: room_temp_zigbee
    entity_id: sensor.living_room_temperature

opentherm:
  id: opentherm_gateway
  in_pin: 4
  out_pin: 5
  slave_in_pin: 12
  slave_out_pin: 13

  # Room-level sensors (intercepted from thermostat WRITE-DATA)
  room_temperature:
    name: "Room Temperature"
  room_setpoint:
    name: "Room Setpoint"

  # Boiler-level sensors (existing, unchanged)
  boiler_temperature:
    name: "Boiler Water Temperature"
  heating_target_temperature:
    name: "CH Water Setpoint"
  external_temperature:
    name: "Outside Temperature"

  # Climate: Boiler water control (existing, unchanged)
  heating_water_climate:
    name: "Central Heating Water"

  # Climate: DHW water control (existing, unchanged)
  hot_water_climate:
    name: "Hot Water"

  # Climate: Room temperature display (NEW - read-only, informational)
  room_climate:
    name: "Room Temperature"
    # Optional: override current_temperature with external HA sensor
    current_temperature_sensor: room_temp_zigbee
```

### Minimal Example: Just Room Sensors (No External Sensor)

```yaml
opentherm:
  id: opentherm_gateway
  in_pin: 4
  out_pin: 5
  slave_in_pin: 12
  slave_out_pin: 13

  # Room temp from thermostat (requires thermostat that sends Tr/TrSet)
  room_temperature:
    name: "Room Temperature"
  room_setpoint:
    name: "Room Setpoint"

  room_climate:
    name: "Room Temperature"
```

### Issue #10 Example: External Sensor Only

```yaml
sensor:
  - platform: homeassistant
    id: room_temp_ha
    entity_id: sensor.living_room_temperature

opentherm:
  id: opentherm_gateway
  in_pin: 4
  out_pin: 5
  slave_in_pin: 12
  slave_out_pin: 13

  room_climate:
    name: "Room Temperature"
    current_temperature_sensor: room_temp_ha
```

---

## Implementation Order

1. **Phase 1** (Steps 1.1-1.3): Room temperature caching + sensors. Low risk, purely additive.
2. **Phase 2** (Steps 2.1-2.4): New room_climate entity. Medium complexity, no changes to existing entities.
3. **Phase 3** (Steps 3.1-3.2): External HA sensor support for room_climate. Requires Phase 2.
4. **Phase 4**: BLOR fix. Independent, can be done anytime.

## Files Changed Per Phase

| Phase | `__init__.py` | `opentherm_component.h` | `opentherm_component.cpp` | `opentherm_climate.h` | `opentherm_climate.cpp` |
|-------|:---:|:---:|:---:|:---:|:---:|
| 1 | Add sensors | Add cache + sensor ptrs | Add cache cases + publish | - | - |
| 2 | Add room_climate | Add room_climate ptr + enum | Add register + update | Add read_only | Add traits + control guard |
| 3 | Extend schema | Add ext sensor ptr | Modify room update | - | - |
| 4 | - | - | Fix sendBoilerReset | - | - |

## Testing Strategy

- **Python validation**: `python -m py_compile components/opentherm/__init__.py`
- **ESPHome config validation**: `esphome config build.yaml`
- **Full compile**: Docker build with `example_opentherm.yaml` updated to include room sensors
- **Edge cases**:
  - Thermostat doesn't send Tr/TrSet -> room_climate current_temp stays NAN (no crash)
  - External HA sensor disconnects -> falls back to OT room temp (Tr)
  - Neither Tr nor external sensor available -> current_temperature stays NAN
  - Startup: all NAN until first data arrives

## Risk Assessment

| Phase | Risk | Rationale |
|-------|------|-----------|
| 1 | **Low** | Purely additive sensors, zero changes to existing behavior |
| 2 | **Low** | New entity, existing entities untouched. Read-only = simple |
| 3 | **Low** | Adds optional config to Phase 2 entity, well-established ESPHome pattern |
| 4 | **Low** | Isolated bug fix in one function |

## Why This Architecture Is Better Than Modifying heating_water_climate

1. **No breaking changes**: Existing users' `heating_water_climate` works exactly as before
2. **Clean separation**: Water-level control vs room-level monitoring are different concerns
3. **Gateway philosophy**: We observe and display, we don't replace the thermostat
4. **Proven pattern**: gduteil and KPWhiver both keep water and room data separate
5. **Simple implementation**: Read-only entity has no complex setpoint translation, no equithermal curves, no PID controllers

## jelenatko's Fork: What We Adopt vs Skip

| From jelenatko | Our plan | Reason |
|---|---|---|
| Tr/TrSet caching | **Adopt** (Phase 1) | Clean, necessary foundation |
| Room temp in climate | **Adopt differently** (Phase 2) | Separate entity, not modifying existing one |
| Equithermal curve | **Skip** | We're a gateway, not a thermostat. Hard-coded constants are fragile |
| Startup protection | **Skip** | Workaround for HA state restoration. Read-only climate doesn't have this problem |
| Override detection | **Skip** | Not needed when room_climate is read-only |
| DHW force-update | **Skip** | Separate issue, can be addressed independently |
