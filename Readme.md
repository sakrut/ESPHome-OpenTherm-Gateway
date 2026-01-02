# ESPHome OpenTherm Gateway

[![build](https://github.com/sakrut/ESPHome-OpenTherm-Gateway/actions/workflows/build.yml/badge.svg)](https://github.com/sakrut/ESPHome-OpenTherm-Gateway/actions/workflows/build.yml)

An ESPHome external component for OpenTherm boilers using [Ihor Melnyk's adapter](http://ihormelnyk.com/opentherm_adapter) or [DIYLESS Gateway](https://diyless.com/product/esp8266-opentherm-gateway). Works as a **man-in-the-middle gateway** between your thermostat and boiler.

## Features

- ✅ **Monitor**: Status (flame, heating, DHW, fault), temperatures, pressure, modulation
- ✅ **Control**: Override heating/DHW temperature setpoints via Home Assistant
- ✅ **Smart persistence**: User-set temperatures don't reset (fixed for heating curves)
- ✅ **Diagnostics**: Max setpoints, OEM fault/diagnostic codes, OpenTherm versions
- ✅ **Reset button**: Remote BLOR (Boiler Lock-Out Reset) command
- ✅ **Smart caching**: 80-90% reduction in OpenTherm bus traffic
- ✅ **Configurable update interval** (default 30s)

## Installation

```yaml
external_components:
  - source: github://sakrut/ESPHome-OpenTherm-Gateway
    components: [ opentherm ]
```

## Quick Start

### Minimal Configuration

```yaml
binary_sensor:
sensor:
climate:

opentherm:
  id: opentherm_gateway
  in_pin: 4           # D2 - to boiler IN
  out_pin: 5          # D1 - to boiler OUT
  slave_in_pin: 12    # D6 - to thermostat OUT
  slave_out_pin: 13   # D7 - to thermostat IN
```

### Full Configuration

See [`example_opentherm.yaml`](example_opentherm.yaml) for complete example.

```yaml
binary_sensor:
sensor:
climate:

opentherm:
  id: opentherm_gateway
  in_pin: 4
  out_pin: 5
  slave_in_pin: 12
  slave_out_pin: 13
  update_interval: 30s  # Optional

  # Binary sensors
  flame:
    name: "Boiler Flame"
  ch_active:
    name: "Central Heating Active"
  dhw_active:
    name: "Hot Water Active"
  fault:
    name: "Boiler Fault"
  diagnostic:
    name: "Boiler Diagnostic"

  # Temperature sensors
  external_temperature:
    name: "External Temperature"
  return_temperature:
    name: "Return Temperature"
  boiler_temperature:
    name: "Boiler Temperature"
  heating_target_temperature:
    name: "Heating Target"  # What boiler actually uses

  # System sensors
  pressure:
    name: "System Pressure"
  modulation:
    name: "Modulation Level"

  # Diagnostics (read once at startup)
  max_ch_setpoint:
    name: "Max CH Setpoint"
  max_modulation:
    name: "Max Modulation"
  master_ot_version:
    name: "Master OT Version"
  slave_ot_version:
    name: "Slave OT Version"

  # Fault codes (when active)
  oem_fault_code:
    name: "OEM Fault Code"
  oem_diagnostic_code:
    name: "OEM Diagnostic Code"

  # Climate controls
  hot_water_climate:
    name: "Hot Water"
  heating_water_climate:
    name: "Central Heating"

# Boiler reset button
button:
  - platform: opentherm
    opentherm_id: opentherm_gateway
    name: "Reset Boiler"
    icon: "mdi:restart-alert"
```

## Wiring (Gateway Mode)

```
[Thermostat] ←→ [ESP Gateway] ←→ [Boiler]
                    ↓
              [Home Assistant]
```

**Boiler side (Master):**
- `in_pin` (4/D2) → Adapter IN
- `out_pin` (5/D1) → Adapter OUT

**Thermostat side (Slave):**
- `slave_in_pin` (12/D6) → Thermostat OUT
- `slave_out_pin` (13/D7) → Thermostat IN

## How It Works

### Temperature Control with Heating Curves

- **Thermostat stays in control** - gateway intercepts and monitors
- **Override via climate entities** - set your own temperature in HA
- **Smart persistence** - your temperature doesn't reset to boiler value
- `heating_target_temperature` sensor shows **what boiler uses**
- Climate entity shows **what you requested**
- Boilers with heating curves calculate water temp based on outdoor temp

### Smart Caching

- Intercepts thermostat↔boiler communication
- Caches responses (60s timeout)
- Only fetches when cache expires
- Rate limiting (5s minimum between fetches)
- **Result: ~80-90% less bus traffic**

## Troubleshooting

### Common Issues

**Climate temperature resets**
- ✅ **Fixed** - Upgrade to latest version
- `heating_target_temperature` sensor shows boiler value
- Climate entity keeps your override

**Sensor shows 0 or doesn't update**
- Enable DEBUG logging (see below)
- Check if thermostat requests these values
- Verify pin wiring

**Build fails (CLIMATE_SCHEMA)**
- Update to ESPHome 2025.12.4+

**Reset doesn't work**
- BLOR only works when boiler is in fault/lockout
- Check DEBUG logs for details

### Debug Logging

```yaml
logger:
  level: DEBUG
  logs:
    opentherm.component: DEBUG
    opentherm.climate: DEBUG
```

Look for:
- `Intercepted msg_id` - What's captured
- `Cached X: Y°C` - Cache working
- `Setting X temperature` - Writes
- `X setpoint verified` - Verification

## Hardware

**Adapters:**
- [Ihor Melnyk OpenTherm adapter](http://ihormelnyk.com/opentherm_adapter)
- [DIYLESS ESP8266 Gateway](https://diyless.com/product/esp8266-opentherm-gateway)

**Tested Boilers:**
- Beretta (with heating curves)
- Various OpenTherm-compatible boilers

## Technical Details

**Architecture:**
- Gateway mode: Master (to boiler) + Slave (from thermostat)
- Interrupt-driven (`IRAM_ATTR`)
- Smart caching with timeout & rate limiting
- Response processing in `loop()` (not interrupt)

**Dependencies:**
- ESPHome 2022.5.0+ (tested 2025.12.4)
- OpenTherm Library 1.1.4 (auto-installed)
- ESP8266 or ESP32

## Documentation

- [`example_opentherm.yaml`](example_opentherm.yaml) - Complete config
- [`CLAUDE.md`](CLAUDE.md) - Architecture & development guide
- [OpenTherm Protocol v2.2](doc/Opentherm%20Protocol%20v2-2.pdf) - Full spec

## Contributing

Contributions welcome! Please:
- Test thoroughly
- Follow existing code style
- Update documentation
- Create PR with clear description

## Credits

- [Ihor Melnyk's OpenTherm Library](https://github.com/ihormelnyk/opentherm_library)
- OpenTherm community

---

![OpenTherm Gateway](https://github.com/user-attachments/assets/26b1cef0-c159-4238-ae4a-82fa8ff81236)
