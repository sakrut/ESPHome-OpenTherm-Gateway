# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESPHome OpenTherm Gateway - An ESPHome external component that acts as a man-in-the-middle gateway between an OpenTherm boiler controller (thermostat) and the boiler itself. It enables monitoring and control of OpenTherm heating systems through Home Assistant integration.

## Key Architecture

### Component Structure

This is an ESPHome external component with the standard structure:
- `components/opentherm/` - Main component directory
  - `__init__.py` - ESPHome component registration and config validation
  - `opentherm_component.{h,cpp}` - Core C++ gateway implementation
  - `opentherm_climate.{h,cpp}` - Climate entity implementation for HA
  - `binary_sensor.py`, `sensor.py`, `climate.py` - Platform integrations

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

### Local Build
```bash
# Create secrets file (required for build)
echo -e "wifi_ssid: 'test_ssid'\nwifi_password: 'test_password'\nesphome_fallback_password: 'fallback'" > secrets.yaml

# Build using Docker (recommended)
docker run --rm -v "${PWD}":/config esphome/esphome compile build.yaml

# Or build example configuration
docker run --rm -v "${PWD}":/config esphome/esphome compile example_opentherm.yaml
```

### CI/CD
The GitHub Actions workflow (`.github/workflows/build.yml`) automatically builds `build.yaml` on push/PR to main.

## Component Dependencies

- ESPHome 2022.5.0 or newer
- OpenTherm Library 1.1.4 (automatically installed via `cg.add_library()`)
- Required ESPHome platforms: binary_sensor, sensor, climate

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

### Adding New Sensors
1. Add constant in `__init__.py` (CONF_*)
2. Add to CONFIG_SCHEMA with appropriate ESPHome schema
3. Add pointer in `opentherm_component.h`
4. Add setter method in `opentherm_component.h`
5. Register in `to_code()` in `__init__.py`
6. Implement reading logic in `update()` in `opentherm_component.cpp`

### OpenTherm Protocol
- Use `ot_->buildRequest()` to construct requests
- Request types: READ, WRITE (from OpenThermRequestType enum)
- Message IDs defined in OpenTherm Library (e.g., OpenThermMessageID::TSet)
- Always check `ot_->isValidResponse()` before processing
- Convert temperatures with `ot_->temperatureToData()` and `ot_->getFloat()`

### Interrupt Safety
All interrupt handlers must be marked `IRAM_ATTR` and access only static members or members via the static `instance_` pointer.
