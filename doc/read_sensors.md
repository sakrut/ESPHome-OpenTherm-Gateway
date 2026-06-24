# Generic OpenTherm Read Sensors

`read_sensors` lets you read arbitrary OpenTherm message IDs without adding a
dedicated option to the component. The component chooses the OpenTherm wire
decoder from `message_id`; you only configure how the value should appear in
Home Assistant.

```yaml
opentherm:
  id: opentherm_gateway

  read_sensors:
    - message_id: 19
      name: "DHW Flow Rate"
      unit_of_measurement: "L/min"
      accuracy_decimals: 2
      state_class: measurement
```

The generic reader supports these decoded forms:

- `f8.8`: decoded as a decimal value.
- `u16`: decoded as an unsigned 16-bit integer.
- `s16`: decoded as a signed 16-bit integer.
- `raw`: exposed as the raw 16-bit payload. Use this for flags, split-byte
  fields, and special fields unless a dedicated sensor is later added.

Not every boiler supports every message ID. Unsupported IDs usually return no
valid value.

`message_id: 0` (`Status`) is special-cased for safety. The component publishes
the last intercepted Status response payload and does not poll the boiler with a
new Status request, because Status request data carries master enable flags.

## Useful Examples

### DHW Flow Rate

Domestic hot water flow rate in litres per minute. Useful to see when hot water
is actually being drawn and how much flow the boiler reports.

```yaml
read_sensors:
  - message_id: 19
    name: "DHW Flow Rate"
    unit_of_measurement: "L/min"
    accuracy_decimals: 2
    state_class: measurement
```

### Flame Current

Electrical current through the burner flame. Useful for ignition and flame
quality diagnostics.

```yaml
read_sensors:
  - message_id: 36
    name: "Flame Current"
    unit_of_measurement: "uA"
    accuracy_decimals: 2
    state_class: measurement
```

### Boiler Power Cycles

Number of boiler power cycles. Useful for detecting power stability problems or
unexpected boiler resets.

```yaml
read_sensors:
  - message_id: 97
    name: "Boiler Power Cycles"
    accuracy_decimals: 0
    state_class: total_increasing
```

### Unsuccessful Burner Starts

Number of unsuccessful burner starts. Useful for service diagnostics.

```yaml
read_sensors:
  - message_id: 113
    name: "Unsuccessful Burner Starts"
    accuracy_decimals: 0
    state_class: total_increasing
```

### Flame Signal Too Low Count

Number of times the flame signal was too low. Useful when diagnosing flame
sensor, gas, or combustion issues.

```yaml
read_sensors:
  - message_id: 114
    name: "Flame Signal Too Low Count"
    accuracy_decimals: 0
    state_class: total_increasing
```

### Successful Burner Starts

Number of successful burner starts.

```yaml
read_sensors:
  - message_id: 116
    name: "Successful Burner Starts"
    accuracy_decimals: 0
    state_class: total_increasing
```

### Burner Operation Hours

Total burner operation hours.

```yaml
read_sensors:
  - message_id: 120
    name: "Burner Operation Hours"
    unit_of_measurement: "h"
    accuracy_decimals: 0
    state_class: total_increasing
```

### CH Pump Operation Hours

Central heating pump operation hours.

```yaml
read_sensors:
  - message_id: 121
    name: "CH Pump Operation Hours"
    unit_of_measurement: "h"
    accuracy_decimals: 0
    state_class: total_increasing
```

### Solar Storage Temperature

Solar storage temperature. Useful only for systems where the boiler reports
solar storage data.

```yaml
read_sensors:
  - message_id: 29
    name: "Solar Storage Temperature"
    unit_of_measurement: "C"
    accuracy_decimals: 1
    device_class: temperature
    state_class: measurement
```

### Solar Collector Temperature

Solar collector temperature. Useful only for solar-capable systems.

```yaml
read_sensors:
  - message_id: 30
    name: "Solar Collector Temperature"
    unit_of_measurement: "C"
    accuracy_decimals: 1
    device_class: temperature
    state_class: measurement
```

### CH2 Flow Temperature

Flow temperature for the second central heating circuit.

```yaml
read_sensors:
  - message_id: 31
    name: "CH2 Flow Temperature"
    unit_of_measurement: "C"
    accuracy_decimals: 1
    device_class: temperature
    state_class: measurement
```

### Boiler Heat Exchanger Temperature

Boiler heat exchanger temperature, if supported by the boiler.

```yaml
read_sensors:
  - message_id: 34
    name: "Boiler Heat Exchanger Temperature"
    unit_of_measurement: "C"
    accuracy_decimals: 1
    device_class: temperature
    state_class: measurement
```

### Relative Humidity

Actual relative humidity as a percentage, if supported.

```yaml
read_sensors:
  - message_id: 38
    name: "Relative Humidity"
    unit_of_measurement: "%"
    accuracy_decimals: 1
    device_class: humidity
    state_class: measurement
```

### Ventilation CO2 Exhaust

CO2 level in exhaust air for ventilation/heat-recovery systems.

```yaml
read_sensors:
  - message_id: 79
    name: "CO2 Exhaust"
    unit_of_measurement: "ppm"
    accuracy_decimals: 0
    device_class: carbon_dioxide
    state_class: measurement
```

### Exhaust Fan RPM

Exhaust fan speed for ventilation/heat-recovery systems.

```yaml
read_sensors:
  - message_id: 84
    name: "Exhaust Fan Speed"
    unit_of_measurement: "rpm"
    accuracy_decimals: 0
    state_class: measurement
```

### Supply Fan RPM

Supply fan speed for ventilation/heat-recovery systems.

```yaml
read_sensors:
  - message_id: 85
    name: "Supply Fan Speed"
    unit_of_measurement: "rpm"
    accuracy_decimals: 0
    state_class: measurement
```

### Electricity Production

Current electricity production in watts, for systems that report producer data.

```yaml
read_sensors:
  - message_id: 111
    name: "Electricity Production"
    unit_of_measurement: "W"
    accuracy_decimals: 0
    device_class: power
    state_class: measurement
```

### Cumulative Electricity Production

Cumulative electricity production in kWh.

```yaml
read_sensors:
  - message_id: 112
    name: "Cumulative Electricity Production"
    unit_of_measurement: "kWh"
    accuracy_decimals: 0
    device_class: energy
    state_class: total_increasing
```

## Full Message ID Reference

This table is intended as a starting point for `read_sensors`. `Generic output`
shows what the current generic reader returns.

For `raw` entries, the value is the raw 16-bit payload. Those messages often
contain flags, two separate 8-bit fields, or special encoded data. They may be
better exposed later by dedicated split sensors.

| ID | Name | Format | Description | Generic output |
|---:|---|---|---|---|
| 0 | Status | flag8/flag8 | Master and slave status flags | raw, from intercepted status only |
| 1 | TSet | f8.8 | CH water temperature control setpoint | f8.8 |
| 2 | MConfigMMemberIDcode | flag8/u8 | Master configuration flags / master member ID code | raw |
| 3 | SConfigSMemberIDcode | flag8/u8 | Slave configuration flags / slave member ID code | raw |
| 4 | RemoteRequest | u8/u8 | Remote request | raw |
| 5 | ASFflags | flag8/u8 | Application-specific fault flags and OEM fault code | raw |
| 6 | RBPflags | flag8/flag8 | Remote boiler parameter transfer enable and read/write flags | raw |
| 7 | CoolingControl | f8.8 | Cooling control signal (%) | f8.8 |
| 8 | TsetCH2 | f8.8 | Control setpoint for second CH circuit | f8.8 |
| 9 | TrOverride | f8.8 | Remote override room setpoint | f8.8 |
| 10 | TSP | u8/u8 | Number of transparent slave parameters supported by slave | raw |
| 11 | TSPindexTSPvalue | u8/u8 | Transparent slave parameter index/value | raw |
| 12 | FHBsize | u8/u8 | Fault history buffer size supported by slave | raw |
| 13 | FHBindexFHBvalue | u8/u8 | Fault history buffer index/value | raw |
| 14 | MaxRelModLevelSetting | f8.8 | Maximum relative modulation level setting (%) | f8.8 |
| 15 | MaxCapacityMinModLevel | u8/u8 | Maximum boiler capacity (kW) / minimum boiler modulation level (%) | raw |
| 16 | TrSet | f8.8 | Room setpoint | f8.8 |
| 17 | RelModLevel | f8.8 | Relative modulation level (%) | f8.8 |
| 18 | CHPressure | f8.8 | CH circuit water pressure (bar) | f8.8 |
| 19 | DHWFlowRate | f8.8 | DHW flow rate (litres/minute) | f8.8 |
| 20 | DayTime | special/u8 | Day of week and time of day | raw |
| 21 | Date | u8/u8 | Calendar date | raw |
| 22 | Year | u16 | Calendar year | u16 |
| 23 | TrSetCH2 | f8.8 | Room setpoint for second CH circuit | f8.8 |
| 24 | Tr | f8.8 | Room temperature | f8.8 |
| 25 | Tboiler | f8.8 | Boiler flow water temperature | f8.8 |
| 26 | Tdhw | f8.8 | DHW temperature | f8.8 |
| 27 | Toutside | f8.8 | Outside temperature | f8.8 |
| 28 | Tret | f8.8 | Return water temperature | f8.8 |
| 29 | Tstorage | f8.8 | Solar storage temperature | f8.8 |
| 30 | Tcollector | f8.8 | Solar collector temperature | f8.8 |
| 31 | TflowCH2 | f8.8 | Flow water temperature for CH2 circuit | f8.8 |
| 32 | Tdhw2 | f8.8 | Domestic hot water temperature 2 | f8.8 |
| 33 | Texhaust | s16 | Boiler exhaust temperature | s16 |
| 34 | TboilerHeatExchanger | f8.8 | Boiler heat exchanger temperature | f8.8 |
| 35 | BoilerFanSpeedSetpointAndActual | u8/u8 | Boiler fan speed setpoint and actual value | raw |
| 36 | FlameCurrent | f8.8 | Electrical current through burner flame | f8.8 |
| 37 | TrCH2 | f8.8 | Room temperature for second CH circuit | f8.8 |
| 38 | RelativeHumidity | f8.8 | Actual relative humidity (%) | f8.8 |
| 39 | TrOverride2 | f8.8 | Remote override room setpoint 2 | f8.8 |
| 48 | TdhwSetUBTdhwSetLB | s8/s8 | DHW setpoint upper/lower bounds | raw |
| 49 | MaxTSetUBMaxTSetLB | s8/s8 | Max CH water setpoint upper/lower bounds | raw |
| 56 | TdhwSet | f8.8 | DHW setpoint | f8.8 |
| 57 | MaxTSet | f8.8 | Max CH water setpoint | f8.8 |
| 70 | StatusVentilationHeatRecovery | flag8/flag8 | Ventilation/heat-recovery status flags | raw |
| 71 | Vset | -/u8 | Relative ventilation position (0-100%) | u16 |
| 72 | ASFflagsOEMfaultCodeVentilationHeatRecovery | flag8/u8 | Ventilation/heat-recovery fault flags and OEM fault code | raw |
| 73 | OEMDiagnosticCodeVentilationHeatRecovery | u16 | Ventilation/heat-recovery OEM diagnostic code | u16 |
| 74 | SConfigSMemberIDCodeVentilationHeatRecovery | flag8/u8 | Ventilation/heat-recovery slave config flags/member ID | raw |
| 75 | OpenThermVersionVentilationHeatRecovery | f8.8 | Ventilation/heat-recovery OpenTherm protocol version | f8.8 |
| 76 | VentilationHeatRecoveryVersion | u8/u8 | Ventilation/heat-recovery product version/type | raw |
| 77 | RelVentLevel | -/u8 | Relative ventilation (0-100%) | u16 |
| 78 | RHexhaust | -/u8 | Relative humidity exhaust air (0-100%) | u16 |
| 79 | CO2exhaust | u16 | CO2 level exhaust air (ppm) | u16 |
| 80 | Tsi | f8.8 | Supply inlet temperature | f8.8 |
| 81 | Tso | f8.8 | Supply outlet temperature | f8.8 |
| 82 | Tei | f8.8 | Exhaust inlet temperature | f8.8 |
| 83 | Teo | f8.8 | Exhaust outlet temperature | f8.8 |
| 84 | RPMexhaust | u16 | Exhaust fan speed (rpm) | u16 |
| 85 | RPMsupply | u16 | Supply fan speed (rpm) | u16 |
| 86 | RBPflagsVentilationHeatRecovery | flag8/flag8 | Ventilation/heat-recovery parameter transfer flags | raw |
| 87 | NominalVentilationValue | u8/- | Nominal relative value for ventilation (0-100%) | raw |
| 88 | TSPventilationHeatRecovery | u8/u8 | Number of ventilation/heat-recovery TSPs | raw |
| 89 | TSPindexTSPvalueVentilationHeatRecovery | u8/u8 | Ventilation/heat-recovery TSP index/value | raw |
| 90 | FHBsizeVentilationHeatRecovery | u8/u8 | Ventilation/heat-recovery fault history buffer size | raw |
| 91 | FHBindexFHBvalueVentilationHeatRecovery | u8/u8 | Ventilation/heat-recovery fault history buffer index/value | raw |
| 93 | Brand | u8/u8 | Brand text string index/character | raw |
| 94 | BrandVersion | u8/u8 | Brand version text string index/character | raw |
| 95 | BrandSerialNumber | u8/u8 | Brand serial number text string index/character | raw |
| 96 | CoolingOperationHours | u16 | Cooling mode operation hours | u16 |
| 97 | PowerCycles | u16 | Number of power cycles | u16 |
| 98 | RFsensorStatusInformation | special/special | RF sensor strength and battery status | raw |
| 99 | RemoteOverrideOperatingModeHeatingDHW | special/special | HC1/HC2 and DHW remote override operating mode | raw |
| 100 | RemoteOverrideFunction | flag8/- | Function of manual/program room setpoint changes | raw |
| 101 | StatusSolarStorage | flag8/flag8 | Solar storage status flags | raw |
| 102 | ASFflagsOEMfaultCodeSolarStorage | flag8/u8 | Solar storage fault flags and OEM fault code | raw |
| 103 | SConfigSMemberIDcodeSolarStorage | flag8/u8 | Solar storage slave config flags/member ID | raw |
| 104 | SolarStorageVersion | u8/u8 | Solar storage product version/type | raw |
| 105 | TSPSolarStorage | u8/u8 | Number of solar storage TSPs | raw |
| 106 | TSPindexTSPvalueSolarStorage | u8/u8 | Solar storage TSP index/value | raw |
| 107 | FHBsizeSolarStorage | u8/u8 | Solar storage fault history buffer size | raw |
| 108 | FHBindexFHBvalueSolarStorage | u8/u8 | Solar storage fault history buffer index/value | raw |
| 109 | ElectricityProducerStarts | u16 | Number of starts of the electricity producer | u16 |
| 110 | ElectricityProducerHours | u16 | Electricity producer operation hours | u16 |
| 111 | ElectricityProduction | u16 | Current electricity production (W) | u16 |
| 112 | CumulativElectricityProduction | u16 | Cumulative electricity production (kWh) | u16 |
| 113 | UnsuccessfulBurnerStarts | u16 | Number of unsuccessful burner starts | u16 |
| 114 | FlameSignalTooLowNumber | u16 | Number of times flame signal was too low | u16 |
| 115 | OEMDiagnosticCode | u16 | OEM diagnostic/service code | u16 |
| 116 | SuccessfulBurnerStarts | u16 | Number of successful burner starts | u16 |
| 117 | CHPumpStarts | u16 | Number of CH pump starts | u16 |
| 118 | DHWPumpValveStarts | u16 | Number of DHW pump/valve starts | u16 |
| 119 | DHWBurnerStarts | u16 | Number of burner starts during DHW mode | u16 |
| 120 | BurnerOperationHours | u16 | Burner operation hours | u16 |
| 121 | CHPumpOperationHours | u16 | CH pump operation hours | u16 |
| 122 | DHWPumpValveOperationHours | u16 | DHW pump/valve operation hours | u16 |
| 123 | DHWBurnerOperationHours | u16 | Burner operation hours during DHW mode | u16 |
| 124 | OpenThermVersionMaster | f8.8 | Master OpenTherm protocol version | f8.8 |
| 125 | OpenThermVersionSlave | f8.8 | Slave OpenTherm protocol version | f8.8 |
| 126 | MasterVersion | u8/u8 | Master product version/type | raw |
| 127 | SlaveVersion | u8/u8 | Slave product version/type | raw |
