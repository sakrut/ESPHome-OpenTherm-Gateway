# Upgrade Guide: 2.0.0 -> 2.1.0

This guide describes what changed from `2.0.0` to `2.1.0` and what you can add to your YAML configuration.

## Summary

Version `2.1.0` adds room-level monitoring features and an optional external temperature source for room climate display:

- New room sensors: `room_temperature`, `room_setpoint`
- New climate entity: `room_climate` (read-only)
- New optional setting: `room_climate.current_temperature_sensor`
- BLOR reset validation now uses OpenTherm `WRITE_ACK` response type

## Is This Breaking?

No. Existing `2.0.0` configurations continue to work.

All new settings are optional.

## What You Can Add in Config (2.0.0 -> 2.1.0)

### 1) Room sensors from OpenTherm traffic

```yaml
opentherm:
  # ...
  room_temperature:
    name: "Room Temperature"
  room_setpoint:
    name: "Room Setpoint"
```

- `room_temperature`: intercepted thermostat room temperature (`Tr`, Data-ID 24)
- `room_setpoint`: intercepted thermostat room setpoint (`TrSet`, Data-ID 16)

### 2) Room climate entity

```yaml
opentherm:
  # ...
  room_climate:
    name: "Room Climate"
```

Behavior:
- `current_temperature`: from OpenTherm `Tr` by default
- `target_temperature`: from OpenTherm `TrSet`
- read-only design (thermostat remains in control)

### 3) Optional external sensor for room climate current temperature

```yaml
sensor:
  - platform: homeassistant
    id: room_temp_ha
    entity_id: sensor.living_room_temperature

opentherm:
  # ...
  room_climate:
    name: "Room Climate"
    current_temperature_sensor: room_temp_ha
```

Priority for `room_climate.current_temperature`:
1. `current_temperature_sensor` (if configured and has state)
2. OpenTherm `Tr` (intercepted value)

## Minimal Migration Example

If you already have `2.0.0` config:

```yaml
opentherm:
  id: opentherm_gateway
  in_pin: 4
  out_pin: 5
  slave_in_pin: 12
  slave_out_pin: 13
  heating_water_climate:
    name: "Central Heating"
```

You can extend it with:

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

  room_temperature:
    name: "Room Temperature"
  room_setpoint:
    name: "Room Setpoint"

  room_climate:
    name: "Room Climate"
    current_temperature_sensor: room_temp_ha

  heating_water_climate:
    name: "Central Heating"
```

## BLOR Reset Note

In `2.1.0`, reset command success is determined by OpenTherm message type (`WRITE_ACK`) instead of payload byte heuristics. This improves reliability of reset success reporting.
