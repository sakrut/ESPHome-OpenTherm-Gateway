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
- `processRequest()` is the core intercept function that forwards requests
- `last_status_response_` caches the most recent status for sensor publishing
- Component polls every 30 seconds (PollingComponent base)
- Interrupt-driven communication via `IRAM_ATTR` handlers

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
- `5` - Application-specific fault flags / OEM fault code
- `14` - Maximum relative modulation level setting
- `17` - Relative modulation level - **MANDATORY**
- `18` - CH water pressure
- `25` - Boiler water temperature - **MANDATORY**
- `26` - DHW temperature (Tdhw)
- `27` - Outside temperature (Toutside)
- `28` - Return water temperature (Tret)
- `56` - DHW setpoint (TdhwSet)
- `57` - Max CH water setpoint
- `115` - OEM diagnostic code
- `124/125` - OpenTherm version (Master/Slave)

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
2. **loop()** - Process slave OpenTherm communication (`slave_ot_->process()`)
3. **update()** - Called every 30 seconds to read sensors and publish states
4. **processRequest()** - Static callback for intercepting thermostat requests

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

### Common OpenTherm Request Patterns
- **Status requests** are periodic and critical - they update `last_status_response_` which feeds all binary sensors
- **Temperature reads** happen in `update()` every 30 seconds
- **Temperature writes** happen immediately when climate entity target is changed
- The component doesn't modify requests from the thermostat, it only intercepts and forwards them
