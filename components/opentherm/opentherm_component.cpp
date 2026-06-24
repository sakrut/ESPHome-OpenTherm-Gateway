#include "opentherm_component.h"
#include "esphome/core/log.h"

namespace esphome
{
  namespace opentherm
  {

    static const char *const TAG = "opentherm.component";

    // Initialize static members
    OpenthermComponent *OpenthermComponent::instance_ = nullptr;
    unsigned long OpenthermComponent::last_status_response_ = 0;
    unsigned long OpenthermComponent::last_intercepted_response_ = 0;
    OpenThermMessageID OpenthermComponent::last_intercepted_id_ = static_cast<OpenThermMessageID>(0);
    bool OpenthermComponent::has_new_intercepted_response_ = false;

    OpenthermComponent::OpenthermComponent(uint32_t update_interval) : PollingComponent(update_interval)
    {
      instance_ = this;
    }

    void OpenthermComponent::add_read_sensor(sensor::Sensor *sensor, uint8_t message_id)
    {
      GenericReadSensor read_sensor;
      read_sensor.sensor = sensor;
      read_sensor.id = static_cast<OpenThermMessageID>(message_id);
      read_sensors_.push_back(read_sensor);
    }

    void OpenthermComponent::setup()
    {
      ESP_LOGD(TAG, "Setting up OpenTherm component");

      // Initialize OpenTherm instances
      ot_ = new OpenTherm(in_pin_, out_pin_, false);                  // Master
      slave_ot_ = new OpenTherm(slave_in_pin_, slave_out_pin_, true); // Slave

      // Start OpenTherm communication
      ot_->begin(handleInterrupt);
      slave_ot_->begin(slaveHandleInterrupt, processRequest);

      // Setup climate controllers
      if (hot_water_climate_ != nullptr)
      {
        hot_water_climate_->set_target_temperature_callback([this](float temperature)
                                                            { return this->setHotWaterTemperature(temperature); });
      }

      if (heating_water_climate_ != nullptr)
      {
        heating_water_climate_->set_target_temperature_callback([this](float temperature)
                                                                { return this->setHeatingTargetTemperature(temperature); });
      }

      // Read Phase 1 values once at startup (these don't change)
      delay(1000); // Give OpenTherm time to initialize

      // Read max CH setpoint (Data-ID 57)
      if (max_ch_setpoint_sensor_ != nullptr)
      {
        unsigned long response = ot_->sendRequest(ot_->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::MaxTSet, 0));
        if (ot_->isValidResponse(response))
        {
          float value = ot_->getFloat(response);
          max_ch_setpoint_sensor_->publish_state(value);
          ESP_LOGI(TAG, "Max CH setpoint: %.1f°C", value);
        }
      }

      // Note: Min CH setpoint (Data-ID 58) is not in standard OpenTherm spec
      // Most boilers don't support it, so we skip it

      // Read max relative modulation (Data-ID 14)
      if (max_modulation_sensor_ != nullptr)
      {
        unsigned long response = ot_->sendRequest(ot_->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::MaxRelModLevelSetting, 0));
        if (ot_->isValidResponse(response))
        {
          float value = ot_->getFloat(response);
          max_modulation_sensor_->publish_state(value);
          ESP_LOGI(TAG, "Max modulation: %.1f%%", value);
        }
      }

      // Read OpenTherm versions (Data-ID 124, 125)
      if (master_ot_version_sensor_ != nullptr)
      {
        unsigned long response = ot_->sendRequest(ot_->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::OpenThermVersionMaster, 0));
        if (ot_->isValidResponse(response))
        {
          float value = ot_->getFloat(response);
          master_ot_version_sensor_->publish_state(value);
          ESP_LOGI(TAG, "Master OT version: %.2f", value);
        }
      }

      if (slave_ot_version_sensor_ != nullptr)
      {
        unsigned long response = ot_->sendRequest(ot_->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::OpenThermVersionSlave, 0));
        if (ot_->isValidResponse(response))
        {
          float value = ot_->getFloat(response);
          slave_ot_version_sensor_->publish_state(value);
          ESP_LOGI(TAG, "Slave OT version: %.2f", value);
        }
      }
    }

    void OpenthermComponent::loop()
    {
      slave_ot_->process();

      // Process intercepted responses (moved from interrupt context)
      if (has_new_intercepted_response_)
      {
        processCachedResponse(last_intercepted_response_, last_intercepted_id_);
        has_new_intercepted_response_ = false;
      }
    }

    void OpenthermComponent::update()
    {
      // Read and publish sensor values

      // Binary sensors from status
      bool is_flame_on = ot_->isFlameOn(last_status_response_);
      bool is_central_heating_active = ot_->isCentralHeatingActive(last_status_response_);
      bool is_hot_water_active = ot_->isHotWaterActive(last_status_response_);
      bool is_fault = ot_->isFault(last_status_response_);
      bool is_diagnostic = ot_->isDiagnostic(last_status_response_);

      if (flame_ != nullptr)
        flame_->publish_state(is_flame_on);

      if (ch_active_ != nullptr)
        ch_active_->publish_state(is_central_heating_active);

      if (dhw_active_ != nullptr)
        dhw_active_->publish_state(is_hot_water_active);

      if (fault_ != nullptr)
        fault_->publish_state(is_fault);

      if (diagnostic_ != nullptr)
        diagnostic_->publish_state(is_diagnostic);

      // Temperature and other sensors (using cache with timeout)
      float ext_temperature = getExternalTemperature();
      float return_temperature = getReturnTemperature();
      float boiler_temperature = getCachedOrFetch(cached_boiler_temp_, OpenThermMessageID::Tboiler);
      float exhaust_temperature = exhaust_temperature_sensor_ != nullptr
                                      ? getCachedOrFetch(cached_exhaust_temp_, OpenThermMessageID::Texhaust)
                                      : NAN;
      float pressure = getPressure();
      float modulation = getModulation();
      float heating_target_temp = getHeatingTargetTemperature();
      float hot_water_temp = getHotWaterTemperature();

      if (external_temperature_sensor_ != nullptr && !std::isnan(ext_temperature))
        external_temperature_sensor_->publish_state(ext_temperature);

      if (return_temperature_sensor_ != nullptr && !std::isnan(return_temperature))
        return_temperature_sensor_->publish_state(return_temperature);

      if (boiler_temperature_ != nullptr && !std::isnan(boiler_temperature))
        boiler_temperature_->publish_state(boiler_temperature);

      if (exhaust_temperature_sensor_ != nullptr && !std::isnan(exhaust_temperature))
        exhaust_temperature_sensor_->publish_state(exhaust_temperature);

      if (pressure_sensor_ != nullptr && !std::isnan(pressure))
        pressure_sensor_->publish_state(pressure);

      if (modulation_sensor_ != nullptr && !std::isnan(modulation))
        modulation_sensor_->publish_state(modulation);

      if (heating_target_temperature_sensor_ != nullptr && !std::isnan(heating_target_temp) && heating_target_temp > 0)
        heating_target_temperature_sensor_->publish_state(heating_target_temp);
      if (room_temperature_sensor_ != nullptr && !std::isnan(cached_room_temp_.value))
        room_temperature_sensor_->publish_state(cached_room_temp_.value);
      if (room_setpoint_sensor_ != nullptr && !std::isnan(cached_room_setpoint_.value))
        room_setpoint_sensor_->publish_state(cached_room_setpoint_.value);

      for (auto &read_sensor : read_sensors_)
      {
        if (read_sensor.sensor == nullptr)
          continue;

        float value = readGenericSensorValue(read_sensor);
        if (!std::isnan(value))
          read_sensor.sensor->publish_state(value);
      }

      // Read OEM diagnostic codes (Data-ID 5 and 115) - only if fault or diagnostic active
      if (is_fault || is_diagnostic)
      {
        // OEM fault code (Data-ID 5) - Application-specific fault flags
        if (oem_fault_code_sensor_ != nullptr)
        {
          unsigned long response = ot_->sendRequest(ot_->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::ASFflags, 0));
          if (ot_->isValidResponse(response))
          {
            uint16_t fault_code = response & 0xFF; // Low byte contains OEM fault code
            oem_fault_code_sensor_->publish_state(fault_code);
            if (fault_code != 0)
            {
              ESP_LOGW(TAG, "OEM Fault Code: %d", fault_code);
            }
          }
        }

        // OEM diagnostic code (Data-ID 115)
        if (oem_diagnostic_code_sensor_ != nullptr)
        {
          unsigned long response = ot_->sendRequest(ot_->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::OEMDiagnosticCode, 0));
          if (ot_->isValidResponse(response))
          {
            uint16_t diag_code = response & 0xFFFF; // Full 16-bit diagnostic code
            oem_diagnostic_code_sensor_->publish_state(diag_code);
            if (diag_code != 0)
            {
              ESP_LOGW(TAG, "OEM Diagnostic Code: %d", diag_code);
            }
          }
        }
      }
      else
      {
        // No fault - publish 0
        if (oem_fault_code_sensor_ != nullptr)
          oem_fault_code_sensor_->publish_state(0);
        if (oem_diagnostic_code_sensor_ != nullptr)
          oem_diagnostic_code_sensor_->publish_state(0);
      }

      // Update climate controllers
      if (hot_water_climate_ != nullptr)
      {
        hot_water_climate_->current_temperature = hot_water_temp;
        hot_water_climate_->action = is_hot_water_active ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_OFF;
        hot_water_climate_->target_temperature = getHotWaterTargetTemperature();
        hot_water_climate_->publish_state();
      }

      if (heating_water_climate_ != nullptr)
      {
        heating_water_climate_->current_temperature = boiler_temperature;
        heating_water_climate_->action = is_central_heating_active ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_OFF;
        // Initialize target temperature from boiler on first update only
        // After that, keep the user's set value and don't overwrite it
        // The heating_target_temperature_sensor will show what the boiler is actually using
        heating_water_climate_->initialize_target_temperature(heating_target_temp);
        heating_water_climate_->publish_state();
      }

      if (room_climate_ != nullptr)
      {
        float room_temperature = NAN;
        if (room_climate_external_sensor_ != nullptr && room_climate_external_sensor_->has_state())
        {
          room_temperature = room_climate_external_sensor_->state;
        }
        else if (!std::isnan(cached_room_temp_.value))
        {
          room_temperature = cached_room_temp_.value;
        }

        if (!std::isnan(room_temperature))
          room_climate_->current_temperature = room_temperature;
        if (!std::isnan(cached_room_setpoint_.value))
          room_climate_->target_temperature = cached_room_setpoint_.value;
        room_climate_->action = is_central_heating_active ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_OFF;
        room_climate_->publish_state();
      }
    }

    void OpenthermComponent::register_climate(OpenthermClimate *climate)
    {
      ClimateType type = climate->get_climate_type();

      if (type == ClimateType::HOT_WATER)
      {
        hot_water_climate_ = climate;
      }
      else if (type == ClimateType::HEATING_WATER)
      {
        heating_water_climate_ = climate;
      }
      else if (type == ClimateType::ROOM)
      {
        room_climate_ = climate;
      }
    }

    float OpenthermComponent::getExternalTemperature()
    {
      return getCachedOrFetch(cached_external_temp_, OpenThermMessageID::Toutside);
    }

    float OpenthermComponent::getHeatingTargetTemperature()
    {
      return getCachedOrFetch(cached_heating_target_, OpenThermMessageID::TSet);
    }

    float OpenthermComponent::getReturnTemperature()
    {
      return getCachedOrFetch(cached_return_temp_, OpenThermMessageID::Tret);
    }

    float OpenthermComponent::getHotWaterTargetTemperature()
    {
      return getCachedOrFetch(cached_dhw_target_, OpenThermMessageID::TdhwSet);
    }

    float OpenthermComponent::getHotWaterTemperature()
    {
      return getCachedOrFetch(cached_dhw_temp_, OpenThermMessageID::Tdhw);
    }

    float OpenthermComponent::getRoomTemperature()
    {
      return cached_room_temp_.value;
    }

    float OpenthermComponent::getRoomSetpoint()
    {
      return cached_room_setpoint_.value;
    }

    bool OpenthermComponent::setTemperatureWithVerification(
        float temperature,
        OpenThermMessageID write_msg_id,
        OpenThermMessageID read_msg_id,
        OpenthermClimate *climate,
        const char *name)
    {
      ESP_LOGI(TAG, "Setting %s temperature to %.1f°C", name, temperature);

      unsigned int data = ot_->temperatureToData(temperature);
      unsigned long request = ot_->buildRequest(OpenThermRequestType::WRITE, write_msg_id, data);
      unsigned long response = ot_->sendRequest(request);

      if (!ot_->isValidResponse(response))
      {
        ESP_LOGE(TAG, "Failed to set %s temperature - invalid response", name);
        return false;
      }

      // Small delay to allow boiler to process the write command
      delay(100);

      // Verify the setpoint was accepted by reading it back (with retry)
      const int max_retries = 3;
      for (int retry = 0; retry < max_retries; retry++)
      {
        unsigned long read_response = ot_->sendRequest(
            ot_->buildRequest(OpenThermRequestType::READ, read_msg_id, 0));

        if (ot_->isValidResponse(read_response))
        {
          float actual_setpoint = ot_->getFloat(read_response);

          if (!std::isnan(actual_setpoint))
          {
            ESP_LOGI(TAG, "%s setpoint verified: %.1f°C (requested: %.1f°C)",
                     name, actual_setpoint, temperature);

            // Update climate entity immediately with verified value
            if (climate != nullptr)
            {
              climate->target_temperature = actual_setpoint;
              climate->publish_state();
            }

            // Check if setpoint was clamped by boiler (e.g., min/max limits)
            if (std::abs(actual_setpoint - temperature) > 1.0f)
            {
              ESP_LOGW(TAG, "%s setpoint was adjusted by boiler from %.1f°C to %.1f°C (min/max limits?)",
                       name, temperature, actual_setpoint);
            }

            return true;
          }
        }

        // Retry with exponential backoff
        if (retry < max_retries - 1)
        {
          unsigned long backoff = 50 * (1 << retry); // 50ms, 100ms, 200ms
          ESP_LOGW(TAG, "Failed to verify %s setpoint, retry %d/%d after %lu ms",
                   name, retry + 1, max_retries, backoff);
          delay(backoff);
        }
      }

      ESP_LOGW(TAG, "%s setpoint write succeeded but verification failed after %d retries", name, max_retries);
      return true; // Write succeeded even if verification failed
    }

    bool OpenthermComponent::setHotWaterTemperature(float temperature)
    {
      return setTemperatureWithVerification(
          temperature,
          OpenThermMessageID::TdhwSet,
          OpenThermMessageID::TdhwSet,
          hot_water_climate_,
          "DHW");
    }

    bool OpenthermComponent::setHeatingTargetTemperature(float temperature)
    {
      return setTemperatureWithVerification(
          temperature,
          OpenThermMessageID::TSet,
          OpenThermMessageID::TSet,
          heating_water_climate_,
          "CH");
    }

    float OpenthermComponent::getModulation()
    {
      return getCachedOrFetch(cached_modulation_, OpenThermMessageID::RelModLevel);
    }

    float OpenthermComponent::getPressure()
    {
      return getCachedOrFetch(cached_pressure_, OpenThermMessageID::CHPressure);
    }

    void OpenthermComponent::processRequest(unsigned long request, OpenThermResponseStatus status)
    {
      if (instance_ != nullptr && instance_->ot_ != nullptr && instance_->slave_ot_ != nullptr)
      {
        unsigned long response = instance_->ot_->sendRequest(request);
        instance_->slave_ot_->sendResponse(response);

        OpenThermMessageID id = instance_->ot_->getDataID(request);
        OpenThermMessageType msg_type = instance_->ot_->getMessageType(request);

        // Log intercepted requests at VERBOSE level
        ESP_LOGV(TAG, "Intercepted msg_id %d (type %d), response valid: %s",
                 static_cast<int>(id),
                 static_cast<int>(msg_type),
                 instance_->ot_->isValidResponse(response) ? "yes" : "no");

        // Update status response (critical for binary sensors)
        if (id == OpenThermMessageID::Status)
        {
          last_status_response_ = response;
        }

        // Store response for processing in loop() (outside interrupt context)
        if (instance_->ot_->isValidResponse(response))
        {
          last_intercepted_response_ = response;
          last_intercepted_id_ = id;
          has_new_intercepted_response_ = true;
        }
        // Also cache WRITE-DATA requests (thermostat setting values)
        else if (msg_type == OpenThermMessageType::WRITE_DATA)
        {
          // For WRITE requests, cache the REQUEST data (what thermostat wants to set)
          last_intercepted_response_ = request; // Use request, not response
          last_intercepted_id_ = id;
          has_new_intercepted_response_ = true;
          ESP_LOGV(TAG, "Caching WRITE-DATA request for msg_id %d", static_cast<int>(id));
        }
      }
    }

    void OpenthermComponent::processCachedResponse(unsigned long response, OpenThermMessageID id)
    {
      // This runs in loop(), not interrupt context - safe to do complex operations
      unsigned long now = millis();
      const CachedMessageSpec *spec = cachedMessageSpec(id);

      if (spec != nullptr)
      {
        CachedValue &cache = this->*(spec->cache);
        cache.value = decodeMessageValue(response, id);
        cache.last_update = now;
        ESP_LOGV(TAG, "Cached %s (msg_id %d): %.2f", spec->log_name, static_cast<int>(id), cache.value);
        return;
      }

      if (id == OpenThermMessageID::Status)
      {
        // Already handled in processRequest for immediate binary sensor updates
        ESP_LOGD(TAG, "Updated status response: %lu", response);
      }
    }

    float OpenthermComponent::decodeMessageValue(unsigned long response, OpenThermMessageID id)
    {
      switch (valueEncodingForMessage(id))
      {
        case ValueEncoding::S16:
          return static_cast<float>(static_cast<int16_t>(ot_->getUInt(response)));

        case ValueEncoding::U16:
        case ValueEncoding::RAW:
          return static_cast<float>(ot_->getUInt(response));

        case ValueEncoding::F88:
        default:
          return ot_->getFloat(response);
      }
    }

    OpenthermComponent::ValueEncoding OpenthermComponent::valueEncodingForMessage(OpenThermMessageID id) const
    {
      switch (static_cast<uint8_t>(id))
      {
        case 0:   // Status: flag8/flag8
        case 2:   // MConfigMMemberIDcode: flag8/u8
        case 3:   // SConfigSMemberIDcode: flag8/u8
        case 4:   // RemoteRequest: u8/u8
        case 5:   // ASFflags: flag8/u8
        case 6:   // RBPflags: flag8/flag8
        case 10:  // TSP: u8/u8
        case 11:  // TSPindexTSPvalue: u8/u8
        case 12:  // FHBsize: u8/u8
        case 13:  // FHBindexFHBvalue: u8/u8
        case 15:  // MaxCapacityMinModLevel: u8/u8
        case 20:  // DayTime: special/u8
        case 21:  // Date: u8/u8
        case 35:  // BoilerFanSpeedSetpointAndActual: u8/u8
        case 48:  // TdhwSetUBTdhwSetLB: s8/s8
        case 49:  // MaxTSetUBMaxTSetLB: s8/s8
        case 70:  // StatusVentilationHeatRecovery: flag8/flag8
        case 72:  // ASFflagsOEMfaultCodeVentilationHeatRecovery: flag8/u8
        case 74:  // SConfigSMemberIDCodeVentilationHeatRecovery: flag8/u8
        case 76:  // VentilationHeatRecoveryVersion: u8/u8
        case 86:  // RBPflagsVentilationHeatRecovery: flag8/flag8
        case 87:  // NominalVentilationValue: u8/-
        case 88:  // TSPventilationHeatRecovery: u8/u8
        case 89:  // TSPindexTSPvalueVentilationHeatRecovery: u8/u8
        case 90:  // FHBsizeVentilationHeatRecovery: u8/u8
        case 91:  // FHBindexFHBvalueVentilationHeatRecovery: u8/u8
        case 93:  // Brand: u8/u8
        case 94:  // BrandVersion: u8/u8
        case 95:  // BrandSerialNumber: u8/u8
        case 98:  // RFsensorStatusInformation: special/special
        case 99:  // RemoteOverrideOperatingModeHeatingDHW: special/special
        case 100: // RemoteOverrideFunction: flag8/-
        case 101: // StatusSolarStorage: flag8/flag8
        case 102: // ASFflagsOEMfaultCodeSolarStorage: flag8/u8
        case 103: // SConfigSMemberIDcodeSolarStorage: flag8/u8
        case 104: // SolarStorageVersion: u8/u8
        case 105: // TSPSolarStorage: u8/u8
        case 106: // TSPindexTSPvalueSolarStorage: u8/u8
        case 107: // FHBsizeSolarStorage: u8/u8
        case 108: // FHBindexFHBvalueSolarStorage: u8/u8
        case 126: // MasterVersion: u8/u8
        case 127: // SlaveVersion: u8/u8
          return ValueEncoding::RAW;

        case 33: // Texhaust: s16
          return ValueEncoding::S16;

        case 71: // Vset: -/u8
        case 77: // RelVentLevel: -/u8
        case 78: // RHexhaust: -/u8
        case 22:  // Year
        case 73:  // OEMDiagnosticCodeVentilationHeatRecovery
        case 79:  // CO2exhaust
        case 84:  // RPMexhaust
        case 85:  // RPMsupply
        case 96:  // CoolingOperationHours
        case 97:  // PowerCycles
        case 109: // ElectricityProducerStarts
        case 110: // ElectricityProducerHours
        case 111: // ElectricityProduction
        case 112: // CumulativElectricityProduction
        case 113: // UnsuccessfulBurnerStarts
        case 114: // FlameSignalTooLowNumber
        case 115: // OEMDiagnosticCode
        case 116: // SuccessfulBurnerStarts
        case 117: // CHPumpStarts
        case 118: // DHWPumpValveStarts
        case 119: // DHWBurnerStarts
        case 120: // BurnerOperationHours
        case 121: // CHPumpOperationHours
        case 122: // DHWPumpValveOperationHours
        case 123: // DHWBurnerOperationHours
          return ValueEncoding::U16;

        default:
          return ValueEncoding::F88;
      }
    }

    const OpenthermComponent::CachedMessageSpec *OpenthermComponent::cachedMessageSpec(OpenThermMessageID id) const
    {
      static const CachedMessageSpec specs[] = {
          {OpenThermMessageID::Toutside, &OpenthermComponent::cached_external_temp_, "external temp"},
          {OpenThermMessageID::Tret, &OpenthermComponent::cached_return_temp_, "return temp"},
          {OpenThermMessageID::Tboiler, &OpenthermComponent::cached_boiler_temp_, "boiler temp"},
          {OpenThermMessageID::Texhaust, &OpenthermComponent::cached_exhaust_temp_, "exhaust temp"},
          {OpenThermMessageID::CHPressure, &OpenthermComponent::cached_pressure_, "pressure"},
          {OpenThermMessageID::RelModLevel, &OpenthermComponent::cached_modulation_, "modulation"},
          {OpenThermMessageID::TSet, &OpenthermComponent::cached_heating_target_, "heating target"},
          {OpenThermMessageID::Tdhw, &OpenthermComponent::cached_dhw_temp_, "DHW temp"},
          {OpenThermMessageID::TdhwSet, &OpenthermComponent::cached_dhw_target_, "DHW target"},
          {OpenThermMessageID::Tr, &OpenthermComponent::cached_room_temp_, "room temp"},
          {OpenThermMessageID::TrSet, &OpenthermComponent::cached_room_setpoint_, "room setpoint"},
      };

      for (const auto &spec : specs)
      {
        if (spec.id == id)
          return &spec;
      }

      return nullptr;
    }

    float OpenthermComponent::getCachedOrFetch(CachedValue &cache, OpenThermMessageID msg_id)
    {
      unsigned long now = millis();

      // Handle first fetch (cache never updated) - last_update will be 0
      if (cache.last_update == 0)
      {
        ESP_LOGV(TAG, "First fetch for msg_id %d", static_cast<int>(msg_id));
        unsigned long response = ot_->sendRequest(ot_->buildRequest(OpenThermRequestType::READ, msg_id, 0));

        if (ot_->isValidResponse(response))
        {
          cache.value = decodeMessageValue(response, msg_id);
          cache.last_update = now;
          ESP_LOGV(TAG, "First fetch for msg_id %d: %.2f", static_cast<int>(msg_id), cache.value);
          return cache.value;
        }
        else
        {
          ESP_LOGW(TAG, "First fetch failed for msg_id %d", static_cast<int>(msg_id));
          cache.last_update = now; // Set timestamp to prevent immediate retry
          return NAN;
        }
      }

      // Unsigned arithmetic handles millis() overflow correctly (wraps at 2^32)
      unsigned long cache_age = now - cache.last_update;

      // Check if cache is fresh (updated within last minute)
      if (!std::isnan(cache.value) && cache_age < CACHE_TIMEOUT_)
      {
        ESP_LOGV(TAG, "Using cached value for msg_id %d: %.2f (age: %lu ms)",
                 static_cast<int>(msg_id), cache.value, cache_age);
        return cache.value;
      }

      // Rate limiting: Don't fetch if we just fetched recently (prevents spam if cache keeps expiring)
      if (cache_age < MIN_FETCH_INTERVAL_)
      {
        ESP_LOGV(TAG, "Rate limited fetch for msg_id %d (last fetch %lu ms ago, min interval %lu ms)",
                 static_cast<int>(msg_id), cache_age, MIN_FETCH_INTERVAL_);
        return cache.value; // Return stale value rather than spam the bus
      }

      // Cache is stale - fetch from boiler
      ESP_LOGV(TAG, "Cache stale for msg_id %d (age: %lu ms), fetching from boiler",
               static_cast<int>(msg_id), cache_age);
      unsigned long response = ot_->sendRequest(ot_->buildRequest(OpenThermRequestType::READ, msg_id, 0));

      if (ot_->isValidResponse(response))
      {
        cache.value = decodeMessageValue(response, msg_id);
        cache.last_update = now;
        ESP_LOGV(TAG, "Fetched value for msg_id %d: %.2f", static_cast<int>(msg_id), cache.value);
        return cache.value;
      }
      else
      {
        ESP_LOGW(TAG, "Failed to fetch value for msg_id %d, using stale cache if available", static_cast<int>(msg_id));
        // Update timestamp even on failure to prevent continuous retry spam
        cache.last_update = now;
      }

      return cache.value; // Return stale value or NAN
    }

    float OpenthermComponent::readGenericSensorValue(GenericReadSensor &read_sensor)
    {
      if (read_sensor.id == OpenThermMessageID::Status)
      {
        if (last_status_response_ == 0)
          return NAN;

        read_sensor.cache.value = decodeMessageValue(last_status_response_, read_sensor.id);
        read_sensor.cache.last_update = millis();
        return read_sensor.cache.value;
      }

      return getCachedOrFetch(read_sensor.cache, read_sensor.id);
    }

    void IRAM_ATTR OpenthermComponent::handleInterrupt()
    {
      if (instance_ != nullptr && instance_->ot_ != nullptr)
      {
        instance_->ot_->handleInterrupt();
      }
    }

    void IRAM_ATTR OpenthermComponent::slaveHandleInterrupt()
    {
      if (instance_ != nullptr && instance_->slave_ot_ != nullptr)
      {
        instance_->slave_ot_->handleInterrupt();
      }
    }

    bool OpenthermComponent::sendBoilerReset()
    {
      ESP_LOGW(TAG, "Sending Boiler Lock-Out Reset (BLOR) command");

      // Build WRITE-DATA command with Command-Code 1 (BLOR) as per OpenTherm spec section 5.3.3
      unsigned long request = ot_->buildRequest(
          OpenThermRequestType::WRITE,
          OpenThermMessageID::Command,  // Data ID 4
          0x0100);                      // HB=1 (BLOR command), LB=0

      ESP_LOGD(TAG, "BLOR request: 0x%08lX", request);
      unsigned long response = ot_->sendRequest(request);
      ESP_LOGD(TAG, "BLOR response: 0x%08lX", response);

      if (ot_->isValidResponse(response))
      {
        OpenThermMessageType resp_type = ot_->getMessageType(response);

        // Extract full response data
        uint16_t response_data = response & 0xFFFF;
        uint8_t high_byte = (response_data >> 8) & 0xFF;
        uint8_t low_byte = response_data & 0xFF;

        ESP_LOGD(TAG, "BLOR response data: HB=0x%02X (%d), LB=0x%02X (%d), type=%d",
                 high_byte, high_byte, low_byte, low_byte, static_cast<int>(resp_type));

        if (resp_type == OpenThermMessageType::WRITE_ACK)
        {
          ESP_LOGI(TAG, "Boiler reset command acknowledged (WRITE-ACK)");
          return true;
        }
        else
        {
          ESP_LOGW(TAG, "BLOR unexpected response type: %d", static_cast<int>(resp_type));
          return false;
        }
      }

      ESP_LOGE(TAG, "Boiler reset command - no valid response");
      return false;
    }

  } // namespace opentherm
} // namespace esphome
