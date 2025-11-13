# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESPHome OpenTherm Gateway - An ESPHome external component that acts as a man-in-the-middle gateway between an OpenTherm boiler controller (thermostat) and the boiler itself. It enables monitoring and control of OpenTherm heating systems through Home Assistant integration.

## Key Architecture

### Component Structure

This is an ESPHome external component with the standard structure:
- `components/opentherm/` - Main component directory
  - `__init__.py` - Main component registration, defines the opentherm hub and all directly attached sensors/binary sensors
  - `opentherm_component.{h,cpp}` - Core C++ gateway implementation
  - `opentherm_climate.{h,cpp}` - Climate entity implementation for HA
  - `sensor.py`, `binary_sensor.py`, `climate.py` - **Separate platform integrations** (alternative configuration style, not used in main examples)

**Important**: This component supports two configuration patterns:
1. **Hub pattern** (used in examples): Define sensors directly under the `opentherm:` component
2. **Platform pattern**: Define sensors separately under `sensor:`, `binary_sensor:`, `climate:` platforms (requires `opentherm_id` reference)

### Gateway Architecture (Critical)

The component operates in **gateway mode** with 4 pins:
- `in_pin`/`out_pin` - Communication with the boiler (acts as Master)
- `slave_in_pin`/`slave_out_pin` - Communication with the thermostat (acts as Slave)

The component intercepts OpenTherm requests from the thermostat, forwards them to the boiler, and relays responses back. This allows monitoring and optional override of setpoints while the original thermostat remains in control.

Key implementation details:
- Two OpenTherm library instances: `ot_` (master) and `slave_ot_` (slave)
- Static singleton pattern (`instance_`) for interrupt handlers
- `processRequest()` is the core intercept function that forwards requests and caches sensor values
- `last_status_response_` caches the most recent status for sensor publishing
- Component polls every 30 seconds (PollingComponent base)
- **Smart caching**: Values intercepted from thermostat↔boiler communication are cached with 1-minute timeout
- Only stale cached values trigger actual requests to the boiler, minimizing bus overhead
- Interrupt-driven communication via `IRAM_ATTR` handlers
- Immediate verification after temperature setpoint changes

### Climate Entity Pattern

`OpenthermClimate` provides Home Assistant climate entities for:
- Hot water temperature control
- Central heating temperature control

Each climate entity uses a callback pattern to communicate temperature changes back to the main component, which then sends OpenTherm WRITE commands.

## Build and Test Commands

### Local Build with Docker (Recommended)
```bash
# Create secrets file (required for build)
echo -e "wifi_ssid: 'test_ssid'\nwifi_password: 'test_password'\nesphome_fallback_password: 'fallback'" > secrets.yaml

# Build using Docker
docker run --rm -v "${PWD}":/config esphome/esphome compile build.yaml

# Build example configuration (includes all sensors)
docker run --rm -v "${PWD}":/config esphome/esphome compile example_opentherm.yaml
```

### Local Build with ESPHome Installed
```bash
# Install ESPHome (if not already installed)
pip install esphome

# Create secrets file
echo -e "wifi_ssid: 'test_ssid'\nwifi_password: 'test_password'\nesphome_fallback_password: 'fallback'" > secrets.yaml

# Compile
esphome compile build.yaml
```

### Validation Without Full Build
```bash
# Validate Python syntax
python -m py_compile components/opentherm/__init__.py
python -m py_compile components/opentherm/sensor.py
python -m py_compile components/opentherm/binary_sensor.py
python -m py_compile components/opentherm/climate.py

# ESPHome config validation (faster than full compile)
esphome config build.yaml
```

### CI/CD
The GitHub Actions workflow (`.github/workflows/build.yml`) automatically builds `build.yaml` on push/PR to main. The workflow:
1. Creates a dummy `secrets.yaml` with placeholder values
2. Builds using the official `esphome/esphome` Docker image
3. Validates the component compiles successfully

## Component Dependencies

- ESPHome 2022.5.0 or newer
- OpenTherm Library 1.1.4 (automatically installed via `cg.add_library()`)
- Required ESPHome platforms: binary_sensor, sensor, climate

## OpenTherm Protocol Documentation

Official OpenTherm Protocol Specification v2.2 is available in [doc/Opentherm Protocol v2-2.pdf](doc/Opentherm Protocol v2-2.pdf). This document contains:
- Physical layer specifications (Manchester encoding, 1000 bits/sec)
- Data-link layer protocol (32-bit frames with parity)
- Application layer with 7 data classes
- Complete list of 128 predefined Data IDs (0-127)

Key protocol facts:
- Point-to-point Master/Slave communication (Room Unit = Master, Boiler = Slave)
- Manchester encoding at 1000 bits/sec nominal
- 32-bit frames: Parity(1) + MsgType(3) + Spare(4) + DataID(8) + DataValue(16)
- Message types: READ-DATA, WRITE-DATA, INVALID-DATA, and corresponding ACK/responses
- Timing: Master must communicate every 1 sec (+15%), Slave responds in 20-800ms
- Two-wire polarity-free connection with concurrent power supply and data

## Configuration Notes

### Pin Configuration
Default pins match NodeMCU/ESP8266 layout:
- in_pin: 4 (D2)
- out_pin: 5 (D1)
- slave_in_pin: 12 (D6)
- slave_out_pin: 13 (D7)

### Important Behavioral Notes
- The original thermostat (Master Controller) remains in control
- Temperature setpoints can be overridden via the climate entities
- If the boiler has heating curves enabled, it may ignore the setpoint override
- All sensors are optional in the configuration
- **Polling interval**: Configurable via `update_interval` (default: 30 seconds) - determines how often sensor states are published to Home Assistant
  - Example: `update_interval: 60s` for slower updates
  - Valid units: `s` (seconds), `min` (minutes), `ms` (milliseconds)
- **Smart caching**: Sensor values are cached from intercepted thermostat↔boiler communication (1-minute timeout)
  - If termostat regularly requests a value, gateway uses the cached value (zero additional bus traffic)
  - If cache expires (no request from thermostat for >1 minute), gateway requests it directly
  - **WRITE-DATA caching**: The component also caches WRITE requests (e.g., when thermostat sets TSet) to capture setpoint changes
- **Immediate feedback**: When setting temperatures via climate entities, the component verifies the setpoint immediately and updates the UI

### Phase 1 Features - Boiler Limits & Diagnostics

The component includes Phase 1 diagnostic and limit sensors that are read automatically:

**Startup-time sensors** (read once during `setup()`):
- `max_ch_setpoint` - Maximum CH water setpoint supported by boiler (Data-ID 57)
- `max_modulation` - Maximum relative modulation level (Data-ID 14)
- `master_ot_version` - OpenTherm protocol version of thermostat (Data-ID 124)
- `slave_ot_version` - OpenTherm protocol version of boiler (Data-ID 125)

**Dynamic diagnostic sensors** (read during `update()` when fault/diagnostic active):
- `oem_fault_code` - Manufacturer-specific fault code (Data-ID 5, low byte)
- `oem_diagnostic_code` - Manufacturer-specific diagnostic code (Data-ID 115)

Example configuration:
```yaml
opentherm:
  id: opentherm_gateway
  # ... pin configuration ...

  # Phase 1 - Boiler Limits (read once at startup)
  max_ch_setpoint:
    name: "Max CH Setpoint"

  max_modulation:
    name: "Max Modulation"

  master_ot_version:
    name: "Master OT Version"

  slave_ot_version:
    name: "Slave OT Version"

  # Phase 1 - Diagnostics (read when fault/diagnostic active)
  oem_fault_code:
    name: "OEM Fault Code"

  oem_diagnostic_code:
    name: "OEM Diagnostic Code"
```

**Benefits**:
- Users can see their boiler's actual maximum temperature limit (helps understand why certain setpoints are rejected)
- Better error reporting with manufacturer-specific diagnostic codes
- Protocol version detection for compatibility checking
- Minimal bus overhead (static values read once, diagnostics only when needed)

### Boiler Reset Button

The component provides an optional reset button that sends the OpenTherm BLOR (Boiler Lock-Out Reset) command. This is useful for:
- Recovering from fault conditions without physical access to the boiler
- Clearing lockout states remotely via Home Assistant
- Can be used in Home Assistant automations for automatic recovery based on sensor conditions

Example configuration:
```yaml
button:
  - platform: opentherm
    opentherm_id: opentherm_gateway
    name: "OpenTherm Reset Boiler"
    icon: "mdi:restart-alert"
```

The button sends a WRITE-DATA command with Data ID 4 (Command) and Command-Code 1 (BLOR) as per OpenTherm Protocol Specification section 5.3.3.

## Testing Configuration

Use `example_opentherm.yaml` as a reference for testing. It includes:
- All available sensors and binary sensors
- Both climate entities
- Template sensors demonstrating how to use the exposed values
- Local external_components configuration for development

For testing as an external component (production usage):
```yaml
external_components:
  - source: github://sakrut/ESPHome-OpenTherm-Gateway
    components: [ opentherm ]
```

### Configuration Options

The component supports the following optional parameters:

```yaml
opentherm:
  id: opentherm_gateway
  # Pin configuration (required)
  in_pin: 4
  out_pin: 5
  slave_in_pin: 12
  slave_out_pin: 13

  # Update interval (optional, default: 30s)
  update_interval: 60s  # Can be in seconds (s), minutes (min), or milliseconds (ms)

  # All sensors, binary sensors, and climate entities are optional
  # See example_opentherm.yaml for complete list
```

## Code Modification Guidelines

### Adding New Sensors to Main Component
When adding sensors to the hub pattern (recommended approach):

1. **In `__init__.py`:**
   - Add constant: `CONF_NEW_SENSOR = "new_sensor"`
   - Add to `CONFIG_SCHEMA` with appropriate `sensor.sensor_schema()` or `binary_sensor.binary_sensor_schema()`
   - Add registration in `to_code()`:
     ```python
     if CONF_NEW_SENSOR in config:
         sens = await sensor.new_sensor(config[CONF_NEW_SENSOR])
         cg.add(var.set_new_sensor(sens))
     ```

2. **In `opentherm_component.h`:**
   - Add pointer: `sensor::Sensor *new_sensor_{nullptr};`
   - Add setter: `void set_new_sensor(sensor::Sensor *sensor) { new_sensor_ = sensor; }`

3. **In `opentherm_component.cpp`:**
   - Implement reading in `update()`:
     ```cpp
     float new_value = getNewValue();  // Add getter if needed
     if (new_sensor_ != nullptr && !std::isnan(new_value))
         new_sensor_->publish_state(new_value);
     ```
   - Add getter method if reading from boiler:
     ```cpp
     float OpenthermComponent::getNewValue() {
         unsigned long response = ot_->sendRequest(
             ot_->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::NewID, 0));
         return ot_->isValidResponse(response) ? ot_->getFloat(response) : NAN;
     }
     ```

### Adding Platform Sensors (Alternative)
For platform-based sensors (separate `sensor:`, `binary_sensor:` sections), modify the respective platform files (`sensor.py`, `binary_sensor.py`, `climate.py`) instead of `__init__.py`. Note: This pattern requires users to reference the hub via `opentherm_id`.

### OpenTherm Protocol Details

**Request Construction:**
```cpp
unsigned long request = ot_->buildRequest(
    OpenThermRequestType::READ,      // or WRITE
    OpenThermMessageID::TSet,        // Message ID from OpenTherm library
    data                              // 0 for READ, actual data for WRITE
);
unsigned long response = ot_->sendRequest(request);
```

**Common Message IDs (from OpenTherm spec):**
- `0` - Status flags (Master and Slave) - **MANDATORY**
- `1` - Control setpoint (TSet) - CH water temp setpoint - **MANDATORY**
- `3` - Slave configuration flags - **MANDATORY**
- `5` - Application-specific fault flags / OEM fault code - **Phase 1: oem_fault_code**
- `14` - Maximum relative modulation level setting - **Phase 1: max_modulation**
- `17` - Relative modulation level - **MANDATORY**
- `18` - CH water pressure
- `25` - Boiler water temperature - **MANDATORY**
- `26` - DHW temperature (Tdhw)
- `27` - Outside temperature (Toutside)
- `28` - Return water temperature (Tret)
- `56` - DHW setpoint (TdhwSet)
- `57` - Max CH water setpoint - **Phase 1: max_ch_setpoint**
- `115` - OEM diagnostic code - **Phase 1: oem_diagnostic_code**
- `124/125` - OpenTherm version (Master/Slave) - **Phase 1: master_ot_version / slave_ot_version**

**Data Classes (Section 5.3 of spec):**
1. Control and Status Information (IDs 0, 1, 5, 8, 115)
2. Configuration Information (IDs 2, 3, 124-127)
3. Remote Commands (ID 4)
4. Sensor and Informational Data (IDs 16-33, 116-123)
5. Pre-Defined Remote Boiler Parameters (IDs 6, 48-63)
6. Transparent Slave Parameters (IDs 10-11)
7. Fault History Data (IDs 12-13)
8. Control of Special Applications (IDs 7, 9, 14-15, 100)

**Response Handling:**
- Always check `ot_->isValidResponse(response)` before using data
- Extract float values: `ot_->getFloat(response)`
- Convert temperatures for WRITE: `ot_->temperatureToData(float_temp)`
- Extract status bits: `ot_->isFlameOn(response)`, `ot_->isCentralHeatingActive(response)`, etc.

### Interrupt Safety
All interrupt handlers must be marked `IRAM_ATTR` and access only static members or members via the static `instance_` pointer. The handlers (`handleInterrupt()`, `slaveHandleInterrupt()`) and callback (`processRequest()`) follow this pattern.

### Component Lifecycle
1. **setup()** - Initialize OpenTherm instances, register climate callbacks
2. **loop()** - Process slave OpenTherm communication (`slave_ot_->process()`) and intercepted responses
3. **update()** - Called at the configured `update_interval` (default: 30s) to read sensors (using cache) and publish states to Home Assistant
4. **processRequest()** - Static callback for intercepting thermostat requests and caching sensor values

### Smart Caching System

The component implements intelligent caching to minimize OpenTherm bus traffic:

**How it works**:
1. All thermostat→boiler communication flows through `processRequest()` (interrupt context)
2. Responses are stored atomically and processed in `loop()` (normal context) to avoid interrupt safety issues
3. When a response contains sensor data (temperature, pressure, modulation, etc.), it's cached with a timestamp
4. When Home Assistant requests sensor updates (every 30s), the component checks cache first:
   - **Cache fresh** (< 1 minute old): Use cached value, **zero bus traffic**
   - **Cache stale but recent** (> 1 min, < 5s since last fetch): Use stale value, **prevent spam**
   - **Cache very stale** (> 5s since last fetch): Request from boiler, update cache

**Cached Data IDs**:
- `Toutside` (27) - External temperature
- `Tret` (28) - Return water temperature
- `Tboiler` (25) - Boiler water temperature
- `CHPressure` (18) - Central heating pressure
- `RelModLevel` (17) - Relative modulation level
- `TSet` (1) - CH water temperature setpoint
- `Tdhw` (26) - DHW temperature
- `TdhwSet` (56) - DHW setpoint

**Safety Features**:
- **Interrupt safety**: Cache updates happen in `loop()`, not interrupt context
- **Rate limiting**: Minimum 5 seconds between fetch requests for the same sensor
- **Millis overflow safe**: Uses unsigned arithmetic that handles 49-day overflow correctly
- **Failure handling**: Failed fetches don't spam the bus (timestamp updated even on failure)

**Benefits**:
- Reduces active polling requests by ~80-90% if thermostat regularly queries these values
- Gateway remains transparent - doesn't add delay to thermostat↔boiler communication
- Fallback polling ensures values are available even if thermostat doesn't request them
- Robust against edge cases (overflow, failures, spam scenarios)

**Configuration** (in [opentherm_component.h](components/opentherm/opentherm_component.h#L132-L133)):
```cpp
const unsigned long CACHE_TIMEOUT_{60000};        // 1 minute - cache freshness
const unsigned long MIN_FETCH_INTERVAL_{5000};    // 5 seconds - rate limit
```

## Debugging and Common Issues

### Enable Debug Logging
Add to your YAML configuration:
```yaml
logger:
  level: DEBUG
  logs:
    opentherm.component: DEBUG
    opentherm.climate: DEBUG
```

### Key Debugging Points
1. **No sensor readings**: Check that `last_status_response_` is being updated in `processRequest()`
2. **Climate controls not working**: Verify the callback is set in `setup()` and that WRITE requests return valid responses
3. **Gateway not intercepting**: Ensure `slave_ot_->process()` is called in `loop()` and pins are correctly configured
4. **Interrupt issues**: Verify IRAM_ATTR marking and static instance pointer access
5. **Sensors stuck at 0 or not updating**:
   - Check cache initialization - there was a bug where `last_update == 0` caused overflow in time calculations
   - Enable VERBOSE logging to see if values are being intercepted and cached
   - Verify thermostat is actually requesting these values (check logs for "Intercepted msg_id")

### Recent Bug Fixes

**Cache Initialization Issue** (Fixed):
- **Problem**: Sensors remained at 0 or didn't update for extended periods
- **Root Cause**: When `last_update == 0`, the expression `now - last_update` resulted in a huge number, triggering rate limiting
- **Solution**: Special handling for `last_update == 0` to immediately fetch value on first access
- **Impact**: Fixes sensors that weren't initializing properly

**TSet WRITE-DATA Caching** (Fixed):
- **Problem**: `heating_target_temperature` stayed at 0 even though thermostat was setting it
- **Root Cause**: Thermostats typically use WRITE (not READ) for TSet, and component only cached READ responses
- **Solution**: `processRequest()` now caches both READ responses AND WRITE requests
- **Impact**: TSet and other setpoint sensors now update when thermostat sets them

**BLOR Enhanced Logging** (Added):
- **Enhancement**: Added detailed hex logging for BLOR command request/response with high/low byte breakdown
- **Purpose**: Help diagnose why boiler reset isn't working (usually boiler isn't in lockout state)
- **Note**: BLOR only works when boiler is actually in lockout/fault state

### Common OpenTherm Request Patterns
- **Status requests** are periodic and critical - they update `last_status_response_` which feeds all binary sensors
- **Temperature reads** use smart caching - if thermostat requested the value recently (<1 min), cached value is used
- **Temperature writes** happen immediately when climate entity target is changed, with immediate verification
- The component doesn't modify requests from the thermostat, it only intercepts, forwards, and caches responses

### Temperature Setpoint Verification

When setting DHW or CH temperatures via climate entities, the component implements robust verification with retry logic:

**Process**:
1. **User sets temperature** in Home Assistant
2. **Component sends WRITE command** to boiler with the requested temperature
3. **100ms delay** - Allows boiler to process the write command
4. **Verification with retry** - Component reads back the actual setpoint from the boiler
   - Up to 3 retry attempts with exponential backoff (50ms, 100ms, 200ms)
   - Handles cases where boiler is slow to update internal state
5. **UI update** - Climate entity is updated with the verified value (not the requested value)
6. **Warning logged** if boiler clamped the value due to min/max limits

**Reliability improvements**:
- Retry logic handles transient failures and timing issues
- Exponential backoff prevents bus spam
- Write success even if verification fails (assumes write worked, logs warning)
- DRY implementation via `setTemperatureWithVerification()` helper

This provides instant, reliable feedback if the boiler rejects or modifies the setpoint (e.g., user sets 20°C but boiler minimum is 35°C).

Example log output:
```
[I][opentherm.component] Setting DHW temperature to 20.0°C
[I][opentherm.component] DHW setpoint verified: 35.0°C (requested: 20.0°C)
[W][opentherm.component] DHW setpoint was adjusted by boiler from 20.0°C to 35.0°C (min/max limits?)
```

Implementation in [opentherm_component.cpp](components/opentherm/opentherm_component.cpp#L177-L265) `setTemperatureWithVerification()` helper.
