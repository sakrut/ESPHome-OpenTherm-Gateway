# Feature Proposals for ESPHome OpenTherm Gateway

Based on OpenTherm Protocol Specification v2.2, here are potential new features to implement:

## 1. **OEM Diagnostic Codes** (Data-ID 115)
**Priority: HIGH** - Very useful for troubleshooting

**What it does:**
- Reads manufacturer-specific fault/diagnostic codes from boiler
- Provides detailed error information beyond standard fault flags
- Example: Baxi boilers have codes like E01, E02, etc.

**Implementation:**
```yaml
sensor:
  - name: "OEM Fault Code"
    id: oem_fault_code
    # Returns manufacturer-specific error code (0-255)
```

**Use case:** When boiler has fault, this shows exact error code for manual lookup

---

## 2. **OpenTherm Version Detection** (Data-ID 124/125)
**Priority: MEDIUM** - Good for compatibility checking

**What it does:**
- Reads OpenTherm protocol version from Master (thermostat) and Slave (boiler)
- Format: X.XX (e.g., 2.30 for v2.3)

**Implementation:**
```yaml
sensor:
  - name: "Master OT Version"
  - name: "Slave OT Version"
```

**Use case:** Verify protocol compatibility, debug communication issues

---

## 3. **Boiler Capacity & Modulation Limits** (Data-ID 6, 14, 57)
**Priority: HIGH** - Essential for proper heating control

**Current issue:** Your boiler rejects 45°C setpoint (minimum is 60°C)
**Solution:** Read these limits from boiler

**Data-IDs:**
- **6**: Remote parameter flags (which parameters can be read/written)
- **14**: Maximum relative modulation level (%)
- **57**: Maximum CH water setpoint (°C)
- **58**: Minimum CH water setpoint (°C) - if supported

**Implementation:**
```yaml
sensor:
  - name: "Max CH Setpoint"
    id: max_ch_setpoint
  - name: "Min CH Setpoint"
    id: min_ch_setpoint
  - name: "Max Modulation"
    id: max_modulation
```

**Use case:**
- Climate entity can dynamically set min/max temperature based on boiler limits
- Prevent user from setting invalid temperatures
- Show actual modulation range

---

## 4. **DHW Flow Rate & Burner Starts** (Data-ID 19, 116, 117, 118, 119)
**Priority: MEDIUM** - Interesting statistics

**What it does:**
- **19**: DHW flow rate (liters/minute)
- **116/117**: Number of burner starts (CH/DHW)
- **118/119**: Number of burner hours (CH/DHW)

**Implementation:**
```yaml
sensor:
  - name: "DHW Flow Rate"
    unit: "L/min"
  - name: "CH Burner Starts"
    state_class: "total_increasing"
  - name: "DHW Burner Starts"
    state_class: "total_increasing"
  - name: "CH Burner Hours"
    unit: "h"
    state_class: "total_increasing"
  - name: "DHW Burner Hours"
    unit: "h"
    state_class: "total_increasing"
```

**Use case:**
- Maintenance scheduling (service after X hours)
- Efficiency monitoring
- Detect frequent cycling issues

---

## 5. **Outside Temperature Compensation** (Data-ID 27)
**Priority: MEDIUM** - Already partially supported

**Current:** We read Toutside but don't write it
**Enhancement:** Allow setting external temperature from HA weather integration

**Implementation:**
```yaml
opentherm:
  # ...existing config...

# In Home Assistant automation:
automation:
  - trigger:
      - platform: state
        entity_id: weather.home
    action:
      - service: esphome.opentherm_gateway_set_external_temp
        data:
          temperature: "{{ state_attr('weather.home', 'temperature') }}"
```

**Use case:**
- Improve heating efficiency with weather-based control
- Boiler adjusts output based on outside temperature

---

## 6. **Remote Boiler Parameters** (Data-ID 48-63)
**Priority: LOW** - Advanced users only

**What it does:**
- Read/write manufacturer-specific parameters
- Examples: pump speed, anti-legionella settings, etc.

**Implementation:**
```yaml
number:
  - platform: opentherm
    name: "Remote Parameter 1"
    data_id: 48
    min_value: 0
    max_value: 255
```

**Use case:** Advanced boiler configuration without physical access

---

## 7. **Solar Storage Temperature** (Data-ID 32)
**Priority: LOW** - Only for solar installations

**What it does:**
- Reads temperature from solar storage tank
- Useful for hybrid boiler+solar systems

---

## 8. **Cooling Support** (Data-ID 7, 9, 31)
**Priority: LOW** - Rare in residential

**What it does:**
- Enable/disable cooling mode
- Read cooling water temperatures
- Control cooling setpoint

**Use case:** Heat pumps with cooling capability

---

## 9. **Ventilation/Heat Recovery** (Data-ID 70-74, 77-78)
**Priority: LOW** - Very rare

**What it does:**
- Control ventilation speed
- Read CO2 levels
- Control heat recovery ventilation (HRV)

---

## 10. **Transparency Slave Parameters** (TSP) - (Data-ID 10, 11)
**Priority: MEDIUM** - Powerful but complex

**What it does:**
- Direct read/write access to ANY boiler parameter
- Requires knowing manufacturer-specific parameter numbers
- Can read things like: pump speed, fan speed, ionization current, etc.

**Example:**
```yaml
sensor:
  - platform: opentherm
    name: "Pump Speed"
    tsp_index: 5  # Manufacturer-specific
    tsp_subindex: 0
```

**Warning:** Can potentially damage boiler if used incorrectly!

---

## Recommendations for Implementation Priority:

### Phase 1 (Essential):
1. **Boiler Capacity & Modulation Limits** - Fix the min/max temperature issue
2. **OEM Diagnostic Codes** - Better error reporting

### Phase 2 (Nice to have):
3. **Burner Statistics** - Maintenance tracking
4. **OpenTherm Version** - Debugging
5. **Outside Temperature Compensation** - Efficiency

### Phase 3 (Advanced):
6. **TSP** - Power users only
7. **Remote Parameters** - Power users only

---

## Configuration Example (Phase 1):

```yaml
opentherm:
  id: opentherm_gateway
  in_pin: 4
  out_pin: 5
  slave_in_pin: 12
  slave_out_pin: 13

  # Existing sensors...

  # NEW: Boiler limits (auto-detected on startup)
  max_ch_setpoint:
    name: "Max CH Temperature Limit"

  min_ch_setpoint:
    name: "Min CH Temperature Limit"

  max_modulation:
    name: "Max Modulation Level"

  # NEW: Diagnostics
  oem_fault_code:
    name: "Boiler Error Code"

  master_ot_version:
    name: "Thermostat OT Version"

  slave_ot_version:
    name: "Boiler OT Version"

  # NEW: Statistics
  ch_burner_starts:
    name: "CH Burner Starts"

  ch_burner_hours:
    name: "CH Burner Hours"

  dhw_burner_starts:
    name: "DHW Burner Starts"

  dhw_burner_hours:
    name: "DHW Burner Hours"

  # Climate entities would auto-adjust their min/max based on detected limits
  heating_water_climate:
    name: "Central Heating"
    # min_temperature and max_temperature now AUTO-DETECTED from boiler!
```

---

## Notes:

- All Data-IDs are optional - boilers may not support all features
- Component should gracefully handle "Unknown-DataID" responses
- Some features (like TSP) need careful documentation about safety
- Boiler limits should be read once at startup and cached

Would you like me to implement any of these features?
