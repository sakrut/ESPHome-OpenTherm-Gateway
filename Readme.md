# ESPHome OpenTherm

[![build](https://github.com/rsciriano/ESPHome-OpenTherm/actions/workflows/build.yml/badge.svg)](https://github.com/rsciriano/ESPHome-OpenTherm/actions/workflows/build.yml)

This is an example of a integration with a OpenTherm boiler using [ESPHome](https://esphome.io/) and the [Ihor Melnyk](http://ihormelnyk.com/opentherm_adapter), the [DIYLESS](https://diyless.com/product/esp8266-opentherm-gateway) as a gateway (Controller -> Gateway -> Boiler)

## Installation
- Copy the content of this repository to your ESPHome folder
- Make sure the pin numbers are right, check the file opentherm_component.h in the esphome-opentherm folder.
- Edit the opentherm.yaml file:
    - Make sure the board and device settings are correct for your device
    - Set dallas address
- Flash the ESP and configure in Home Assistant. It should be auto-discovered by the ESPHome Integration.

## Additional info
In this example, the Master Controller still controls the boiler, You can setup target temperature for heating water and hot water, but in case of heating water if your boiler have turn on heating curve then you can't setup target it is managed only by boiler.