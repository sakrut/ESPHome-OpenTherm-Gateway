# ESPHome OpenTherm Gateway

[![build](https://github.com/sakrut/ESPHome-OpenTherm-Gateway/actions/workflows/build.yml/badge.svg)](https://github.com/sakrut/ESPHome-OpenTherm-Gateway/actions/workflows/build.yml)

An ESPHome external component for integrating with OpenTherm boilers using [Ihor Melnyk's OpenTherm adapter](http://ihormelnyk.com/opentherm_adapter) or [DIYLESS ESP8266 OpenTherm Gateway](https://diyless.com/product/esp8266-opentherm-gateway). This component works as a **gateway** between your boiler controller (thermostat) and your boiler (man-in-the-middle mode).

## Features

### Core Monitoring
- ✅ Monitor boiler status (flame, heating, hot water, fault, diagnostic)
- ✅ Read temperature sensors (external, return, boiler, DHW)
- ✅ Monitor system pressure and modulation level
- ✅ Track heating target temperature from thermostat

### Climate Control
- ✅ Control heating water temperature setpoint (overrides thermostat when needed)
- ✅ Control hot water (DHW) temperature setpoint
- ✅ **Smart temperature persistence** - user-set temperatures don't reset to boiler values
- ✅ Automatic verification after temperature changes (with retry logic)
- ✅ Compatible with boilers using heating curves

### Diagnostics & Limits (Phase 1)
- ✅ **Max CH setpoint** - See your boiler's maximum temperature limit (Data-ID 57)
- ✅ **Max modulation** - Maximum relative modulation level (Data-ID 14)
- ✅ **OEM fault code** - Manufacturer-specific fault code (Data-ID 5)
- ✅ **OEM diagnostic code** - Detailed diagnostic information (Data-ID 115)
- ✅ **OpenTherm versions** - Protocol versions for thermostat and boiler (Data-ID 124/125)

### Advanced Features
- ✅ **Boiler reset button** - Remote BLOR (Boiler Lock-Out Reset) command via Home Assistant
- ✅ **Smart caching system** - Reduces OpenTherm bus traffic by 80-90%
- ✅ **Configurable update interval** - Default 30s, customizable
- ✅ **Interrupt-safe** - Proper IRAM_ATTR handling for ESP8266/ESP32
- ✅ Full integration with Home Assistant via ESPHome

## Installation

### Method 1: Using ESPHome External Components (Recommended)

Add this to your ESPHome YAML configuration:

```yaml
external_components:
  - source: github://sakrut/ESPHome-OpenTherm-Gateway
    components: [ opentherm ]
    refresh: 0d  # Optional: disable caching for development
```

### Method 2: Manual Installation

1. Create a directory called `custom_components` in your ESPHome configuration folder
2. Clone this repository or download and extract it to this directory:
   ```bash
   cd <your_esphome_config_dir>
   mkdir -p custom_components
   cd custom_components
   git clone https://github.com/sakrut/ESPHome-OpenTherm-Gateway opentherm
   ```

## Configuration

### Minimal Configuration

```yaml
# Required platform declarations
binary_sensor:
sensor:
climate:

opentherm:
  id: opentherm_gateway
  in_pin: 4           # D2 on NodeMCU - to boiler adapter IN
  out_pin: 5          # D1 on NodeMCU - to boiler adapter OUT
  slave_in_pin: 12    # D6 on NodeMCU - to thermostat OUT
  slave_out_pin: 13   # D7 on NodeMCU - to thermostat IN
```

### Full Configuration Example

See [`example_opentherm.yaml`](example_opentherm.yaml) for a complete configuration with all available sensors and features.

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

  # Optional: Update interval (default: 30s)
  update_interval: 30s

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
    name: "Heating Target Temperature"  # What the boiler is actually using

  # System sensors
  pressure:
    name: "System Pressure"
  modulation:
    name: "Modulation Level"

  # Phase 1: Boiler limits (read once at startup)
  max_ch_setpoint:
    name: "Max CH Setpoint"
  max_modulation:
    name: "Max Modulation"
  master_ot_version:
    name: "Master OpenTherm Version"
  slave_ot_version:
    name: "Slave OpenTherm Version"

  # Phase 1: Diagnostics (read when fault/diagnostic active)
  oem_fault_code:
    name: "OEM Fault Code"
  oem_diagnostic_code:
    name: "OEM Diagnostic Code"

  # Climate controls
  hot_water_climate:
    name: "Hot Water"
  heating_water_climate:
    name: "Central Heating"

# Optional: Boiler reset button
button:
  - platform: opentherm
    opentherm_id: opentherm_gateway
    name: "Reset Boiler"
    icon: "mdi:restart-alert"
```

## Wiring

### Gateway Mode (4-pin setup)

This component operates as a **man-in-the-middle gateway**:

**Boiler side (Master mode):**
- `in_pin` (4/D2) → OT adapter IN (to boiler)
- `out_pin` (5/D1) → OT adapter OUT (to boiler)

**Thermostat side (Slave mode):**
- `slave_in_pin` (12/D6) → Thermostat OUT
- `slave_out_pin` (13/D7) → Thermostat IN

```
[Thermostat] ←→ [ESP Gateway] ←→ [Boiler]
                    ↓
              [Home Assistant]
```

The gateway intercepts OpenTherm communication, allowing monitoring and optional temperature overrides while keeping your original thermostat in control.

## Important Notes

### Temperature Control with Heating Curves

- Your **original thermostat remains in control** of the heating system
- You can **override temperature setpoints** via the climate entities
- If your boiler has **heating curves enabled**, it calculates water temperature based on outdoor temperature
- The `heating_target_temperature` sensor shows what the **boiler is actually using**
- The climate entity shows what **you requested** (your override stays persistent)
- Temperature changes are **verified automatically** with retry logic

### Smart Caching

The component uses intelligent caching to minimize OpenTherm bus traffic:
- Values intercepted from thermostat↔boiler communication are cached (60s timeout)
- If the thermostat regularly requests a value, the gateway uses cached data (**zero additional bus traffic**)
- If cache expires, the gateway fetches directly from the boiler
- Rate limiting prevents bus spam (minimum 5s between fetches)
- **Result: ~80-90% reduction in active polling requests**

### Boiler Reset (BLOR)

The reset button sends the OpenTherm BLOR (Boiler Lock-Out Reset) command:
- Useful for recovering from fault conditions remotely
- Can be used in Home Assistant automations
- **Only works when the boiler is actually in lockout/fault state**
- See detailed hex logging in DEBUG mode to diagnose issues

## Troubleshooting

### Common Issues

1. **Climate temperature resets to boiler value**
   - ✅ **Fixed in latest version** - user-set temperatures now persist
   - The `heating_target_temperature` sensor shows the boiler's calculated value
   - Climate entity maintains your override

2. **Sensor shows 0 or doesn't update**
   - Enable `DEBUG` logging for `opentherm.component`
   - Check if values are being intercepted (look for "Cached" messages)
   - Verify your thermostat is requesting these values
   - Ensure pins are correctly wired

3. **Build fails with CLIMATE_SCHEMA error**
   - Update to ESPHome 2025.12.4 or newer
   - The component uses `climate.climate_schema()` for compatibility

4. **Boiler reset doesn't work**
   - BLOR only works when boiler is in lockout/fault state
   - Check BLOR hex logs in DEBUG mode
   - Verify boiler supports remote reset

### Debug Logging

Enable detailed logging in your YAML:

```yaml
logger:
  level: DEBUG
  logs:
    opentherm.component: DEBUG
    opentherm.climate: DEBUG
```

Look for:
- `Intercepted msg_id` - Shows what's being captured from thermostat↔boiler
- `Cached X: Y°C` - Confirms values are being cached
- `Using cached value` - Cache is working
- `Setting X temperature to Y°C` - Temperature writes
- `X setpoint verified: Y°C` - Verification after write

## Hardware Compatibility

### Tested OpenTherm Adapters
- [Ihor Melnyk's OpenTherm adapter](http://ihormelnyk.com/opentherm_adapter)
- [DIYLESS ESP8266 OpenTherm Gateway](https://diyless.com/product/esp8266-opentherm-gateway)

### Tested Boilers
- Beretta (with heating curves)
- Various OpenTherm-compatible boilers

**Note:** This component is designed for **OpenTherm protocol only**. Check your boiler manual to confirm OpenTherm support.

## Architecture

### Gateway Mode
- Two OpenTherm instances: one as Master (to boiler), one as Slave (from thermostat)
- Interrupt-driven communication (`IRAM_ATTR` handlers)
- Static singleton pattern for interrupt safety
- `processRequest()` intercepts and forwards all thermostat→boiler communication
- Response caching happens in `loop()` (not interrupt context) for safety

### Smart Caching
- Values from intercepted READ-DATA responses are cached
- WRITE-DATA requests (e.g., thermostat setting TSet) are also cached
- Cache timeout: 60 seconds
- Rate limiting: 5 seconds minimum between fetches
- Handles `millis()` overflow correctly (49-day wraparound safe)

## Dependencies

- **ESPHome** 2022.5.0 or newer (tested with 2025.12.4)
- **OpenTherm Library** 1.1.4 (automatically installed via `cg.add_library()`)
- **ESP8266** or **ESP32** (tested on ESP8266/NodeMCU)

## Documentation

- [`CLAUDE.md`](CLAUDE.md) - Detailed architecture and development guide
- [`example_opentherm.yaml`](example_opentherm.yaml) - Complete configuration example
- [OpenTherm Protocol Specification v2.2](doc/Opentherm%20Protocol%20v2-2.pdf) - Full protocol documentation

## Contributing

Contributions are welcome! Please:
1. Test your changes thoroughly
2. Follow the existing code style
3. Update documentation if adding features
4. Create a pull request with a clear description

## License

This project is open source. See the repository for license details.

## Credits

- Based on [Ihor Melnyk's OpenTherm Library](https://github.com/ihormelnyk/opentherm_library)
- Gateway architecture inspired by the OpenTherm community

---

![OpenTherm Gateway Diagram](https://github.com/user-attachments/assets/26b1cef0-c159-4238-ae4a-82fa8ff81236)
