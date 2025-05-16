# ESPHome OpenTherm Gateway

[![build](https://github.com/sakrut/ESPHome-OpenTherm-Gateway/actions/workflows/build.yml/badge.svg)](https://github.com/sakrut/ESPHome-OpenTherm-Gateway/actions/workflows/build.yml)

An ESPHome external component for integrating with OpenTherm boilers using [Ihor Melnyk's OpenTherm adapter](http://ihormelnyk.com/opentherm_adapter) or [DIYLESS ESP8266 OpenTherm Gateway](https://diyless.com/product/esp8266-opentherm-gateway). This component works as a gateway between your boiler controller and your boiler (man in the middle).

## Features

- Monitor boiler status (flame, heating, hot water)
- Read temperature sensors (external, return, boiler)
- Control heating and hot water temperature setpoints
- Monitor faults and diagnostics
- Full integration with Home Assistant via ESPHome

## Installation

### Method 1: Using ESPHome External Components

Add this to your ESPHome YAML configuration:

```yaml
external_components:
  - source: github://sakrut/ESPHome-OpenTherm-Gateway
    components: [ opentherm ]
```

### Method 2: Manual Installation

1. Create a directory called `custom_components` in your ESPHome configuration folder
2. Clone this repository or download and extract it to this directory:
   ```
   cd <your_esphome_config_dir>
   mkdir -p custom_components
   cd custom_components
   git clone https://github.com/sakrut/ESPHome-OpenTherm-Gateway opentherm
   ```

## Configuration

Add the following to your ESPHome configuration:

```yaml
# Example configuration
climate:
binary_sensor:

opentherm:
  id: opentherm_gateway
  in_pin: 4      # D2 on ESP8266/NodeMCU
  out_pin: 5     # D1 on ESP8266/NodeMCU
  slave_in_pin: 12  # D6 on ESP8266/NodeMCU  
  slave_out_pin: 13 # D7 on ESP8266/NodeMCU
  
  # Optional sensors
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
  external_temperature:
    name: "External Temperature"
  return_temperature:
    name: "Return Temperature"
  boiler_temperature:
    name: "Boiler Temperature"
  pressure:
    name: "System Pressure"
  modulation:
    name: "Modulation Level"
  heating_target_temperature:
    name: "Heating Target Temperature"

  # Climate controls
  hot_water_climate:
    name: "Hot Water"
  heating_water_climate:
    name: "Central Heating"
```

## Wiring

Connect your ESP device to the OpenTherm adapter according to these pins (adjust in your configuration if different):

- in_pin: Connect to the OT adapter's IN terminal
- out_pin: Connect to the OT adapter's OUT terminal
- slave_in_pin: Connect to the thermostat's OT OUT terminal
- slave_out_pin: Connect to the thermostat's OT IN terminal

## Important Notes

- In this setup, the Master Controller still controls the boiler
- You can set the target temperature for both heating water and hot water
- If your boiler has heating curves enabled, the target heating water temperature will be managed by the boiler regardless of your setpoint

## Troubleshooting

If you encounter issues:
1. Check that the pin configurations match your wiring
2. Verify the OpenTherm adapter is properly connected to both the ESP and the boiler
3. Check ESPHome logs for communication errors
4. Ensure the OpenTherm library is properly installed


![image](https://github.com/user-attachments/assets/26b1cef0-c159-4238-ae4a-82fa8ff81236)

## Dependencies

This component requires:
- ESPHome 2022.5.0 or newer
- OpenTherm Library 1.1.4 (automatically installed)
