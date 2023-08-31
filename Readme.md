# ESPHome OpenTherm

[![build](https://github.com/rsciriano/ESPHome-OpenTherm/actions/workflows/build.yml/badge.svg)](https://github.com/rsciriano/ESPHome-OpenTherm/actions/workflows/build.yml)

This is an example of a integration with a OpenTherm boiler using [ESPHome](https://esphome.io/) and the [Ihor Melnyk](http://ihormelnyk.com/opentherm_adapter), the [DIYLESS](https://diyless.com/product/esp8266-opentherm-gateway) as a gateway (Controller -> Gateway -> Boiler)

## Installation
- Copy the content of this repository to your ESPHome folder
- Make sure the pin numbers are right, check the file opentherm_component.h in the esphome-opentherm folder.
- Edit the opentherm.yaml file:
    - Make sure the board and device settings are correct for your device
    - Set the sensor entity_id with the external temperature sensor's name from Home Assistant. (The ESPHome sensor name is temperature_sensor).
- Flash the ESP and configure in Home Assistant. It should be auto-discovered by the ESPHome Integration.

## Additional info
In this example, the Controller still controls the boiler, but we can read data from the boiler. 
We're working on accurately reading data about the set point for hot water and allowing the injection of set points.