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
      {
        flame_->publish_state(is_flame_on);
        ESP_LOGD(TAG, "[HA_CMD] action=publish_binary_sensor name=flame state=%d", is_flame_on ? 1 : 0);
      }

      if (ch_active_ != nullptr)
      {
        ch_active_->publish_state(is_central_heating_active);
        ESP_LOGD(TAG, "[HA_CMD] action=publish_binary_sensor name=ch_active state=%d", is_central_heating_active ? 1 : 0);
      }

      if (dhw_active_ != nullptr)
      {
        dhw_active_->publish_state(is_hot_water_active);
        ESP_LOGD(TAG, "[HA_CMD] action=publish_binary_sensor name=dhw_active state=%d", is_hot_water_active ? 1 : 0);
      }

      if (fault_ != nullptr)
      {
        fault_->publish_state(is_fault);
        ESP_LOGD(TAG, "[HA_CMD] action=publish_binary_sensor name=fault state=%d", is_fault ? 1 : 0);
      }

      if (diagnostic_ != nullptr)
      {
        diagnostic_->publish_state(is_diagnostic);
        ESP_LOGD(TAG, "[HA_CMD] action=publish_binary_sensor name=diagnostic state=%d", is_diagnostic ? 1 : 0);
      }

      // Temperature and other sensors (using cache with timeout)
      float ext_temperature = getExternalTemperature();
      float return_temperature = getReturnTemperature();
      float boiler_temperature = getCachedOrFetch(cached_boiler_temp_, OpenThermMessageID::Tboiler);
      float pressure = getPressure();
      float modulation = getModulation();
      float heating_target_temp = getHeatingTargetTemperature();
      float hot_water_temp = getHotWaterTemperature();

      if (external_temperature_sensor_ != nullptr && !std::isnan(ext_temperature))
      {
        external_temperature_sensor_->publish_state(ext_temperature);
        ESP_LOGD(TAG, "[HA_CMD] action=publish_sensor name=external_temperature value=%.2f unit=°C", ext_temperature);
      }

      if (return_temperature_sensor_ != nullptr && !std::isnan(return_temperature))
      {
        return_temperature_sensor_->publish_state(return_temperature);
        ESP_LOGD(TAG, "[HA_CMD] action=publish_sensor name=return_temperature value=%.2f unit=°C", return_temperature);
      }

      if (boiler_temperature_ != nullptr && !std::isnan(boiler_temperature))
      {
        boiler_temperature_->publish_state(boiler_temperature);
        ESP_LOGD(TAG, "[HA_CMD] action=publish_sensor name=boiler_temperature value=%.2f unit=°C", boiler_temperature);
      }

      if (pressure_sensor_ != nullptr && !std::isnan(pressure))
      {
        pressure_sensor_->publish_state(pressure);
        ESP_LOGD(TAG, "[HA_CMD] action=publish_sensor name=pressure value=%.2f unit=bar", pressure);
      }

      if (modulation_sensor_ != nullptr && !std::isnan(modulation))
      {
        modulation_sensor_->publish_state(modulation);
        ESP_LOGD(TAG, "[HA_CMD] action=publish_sensor name=modulation value=%.2f unit=%%", modulation);
      }

      if (heating_target_temperature_sensor_ != nullptr && !std::isnan(heating_target_temp))
      {
        heating_target_temperature_sensor_->publish_state(heating_target_temp);
        ESP_LOGD(TAG, "[HA_CMD] action=publish_sensor name=heating_target_temperature value=%.2f unit=°C", heating_target_temp);
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

        ESP_LOGD(TAG, "[HA_CMD] action=publish_climate entity=hot_water current_temp=%.2f target_temp=%.2f mode=%s",
                 hot_water_temp,
                 hot_water_climate_->target_temperature,
                 is_hot_water_active ? "heating" : "off");
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

        ESP_LOGD(TAG, "[HA_CMD] action=publish_climate entity=central_heating current_temp=%.2f target_temp=%.2f mode=%s",
                 boiler_temperature,
                 heating_water_climate_->target_temperature,
                 is_central_heating_active ? "heating" : "off");
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
      unsigned long response = ot_->sendRequest(ot_->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::Tr, 0));
      return ot_->isValidResponse(response) ? ot_->getFloat(response) : NAN;
    }

    bool OpenthermComponent::setTemperatureWithVerification(
        float temperature,
        OpenThermMessageID write_msg_id,
        OpenThermMessageID read_msg_id,
        OpenthermClimate *climate,
        const char *name)
    {
      ESP_LOGD(TAG, "[HA_CMD] action=set_temperature entity=%s target=%.1f unit=°C", name, temperature);

      unsigned int data = ot_->temperatureToData(temperature);
      unsigned long request = ot_->buildRequest(OpenThermRequestType::WRITE, write_msg_id, data);

      ESP_LOGD(TAG, "[HA_CMD] action=ot_write msg_id=%d req_data=0x%04X req_raw=0x%08lX",
               static_cast<int>(write_msg_id), data, request);
      ESP_LOGV(TAG, "[OT_PKT] dir=GatewayToBoiler type=ha_action msg_id=%d msg_type=WRITE req_raw=0x%08lX",
               static_cast<int>(write_msg_id), request);

      unsigned long response = ot_->sendRequest(request);
      bool is_valid = ot_->isValidResponse(response);

      if (is_valid)
      {
        uint16_t resp_data = response & 0xFFFF;
        ESP_LOGD(TAG, "[HA_CMD] action=ot_write_resp msg_id=%d resp_raw=0x%08lX resp_data=0x%04X valid=1",
                 static_cast<int>(write_msg_id), response, resp_data);
        ESP_LOGV(TAG, "[OT_PKT] dir=BoilerToGateway type=ha_action msg_id=%d msg_type=WRITE_ACK resp_raw=0x%08lX resp_data=0x%04X valid=1",
                 static_cast<int>(write_msg_id), response, resp_data);
      }
      else
      {
        ESP_LOGE(TAG, "[HA_CMD] action=ot_write_resp msg_id=%d resp_raw=0x%08lX valid=0 error=invalid_response",
                 static_cast<int>(write_msg_id), response);
        ESP_LOGV(TAG, "[OT_PKT] dir=BoilerToGateway type=ha_action msg_id=%d resp_raw=0x%08lX valid=0",
                 static_cast<int>(write_msg_id), response);
        return false;
      }

      // Small delay to allow boiler to process the write command
      delay(100);

      // Verify the setpoint was accepted by reading it back (with retry)
      const int max_retries = 3;
      for (int retry = 0; retry < max_retries; retry++)
      {
        unsigned long read_request = ot_->buildRequest(OpenThermRequestType::READ, read_msg_id, 0);
        ESP_LOGV(TAG, "[OT_PKT] dir=GatewayToBoiler type=verify msg_id=%d msg_type=READ req_raw=0x%08lX retry=%d",
                 static_cast<int>(read_msg_id), read_request, retry);

        unsigned long read_response = ot_->sendRequest(read_request);

        if (ot_->isValidResponse(read_response))
        {
          float actual_setpoint = ot_->getFloat(read_response);

          uint16_t resp_data = read_response & 0xFFFF;
          ESP_LOGV(TAG, "[OT_PKT] dir=BoilerToGateway type=verify msg_id=%d msg_type=READ_ACK resp_raw=0x%08lX resp_data=0x%04X parsed=%.2f valid=1",
                   static_cast<int>(read_msg_id), read_response, resp_data, actual_setpoint);

          if (!std::isnan(actual_setpoint))
          {
            bool match = std::abs(actual_setpoint - temperature) <= 1.0f;
            ESP_LOGD(TAG, "[HA_CMD] action=verify_setpoint msg_id=%d actual=%.1f requested=%.1f match=%d",
                     static_cast<int>(read_msg_id), actual_setpoint, temperature, match ? 1 : 0);

            // Update climate entity immediately with verified value
            if (climate != nullptr)
            {
              climate->target_temperature = actual_setpoint;
              climate->publish_state();
              ESP_LOGD(TAG, "[HA_CMD] action=update_climate entity=%s target=%.1f", name, actual_setpoint);
            }

            // Check if setpoint was clamped by boiler (e.g., min/max limits)
            if (!match)
            {
              ESP_LOGW(TAG, "[HA_CMD] action=setpoint_adjusted entity=%s requested=%.1f actual=%.1f reason=min_max_limits",
                       name, temperature, actual_setpoint);
            }

            return true;
          }
        }

        // Retry with exponential backoff
        if (retry < max_retries - 1)
        {
          unsigned long backoff = 50 * (1 << retry); // 50ms, 100ms, 200ms
          ESP_LOGW(TAG, "[HA_CMD] action=verify_failed msg_id=%d retry=%d/%d backoff_ms=%lu",
                   static_cast<int>(read_msg_id), retry + 1, max_retries, backoff);
          delay(backoff);
        }
      }

      ESP_LOGW(TAG, "[HA_CMD] action=verification_failed entity=%s status=write_succeeded_verify_failed retries=%d",
               name, max_retries);
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
        OpenThermMessageID id = instance_->ot_->getDataID(request);
        OpenThermMessageType msg_type = instance_->ot_->getMessageType(request);

        // VERBOSE: Log intercepted request (Master->Slave)
        ESP_LOGV(TAG, "[OT_PKT] dir=MasterToSlave type=intercept msg_id=%d msg_type=%s req_raw=0x%08lX",
                 static_cast<int>(id),
                 instance_->getMessageTypeName(msg_type),
                 request);

        unsigned long response = instance_->ot_->sendRequest(request);
        instance_->slave_ot_->sendResponse(response);

        bool is_valid = instance_->ot_->isValidResponse(response);

        // VERBOSE: Log intercepted response (Slave->Master) with parsed value
        if (is_valid)
        {
          float parsed_value = instance_->ot_->getFloat(response);
          uint16_t resp_data = response & 0xFFFF;
          uint8_t hb = (resp_data >> 8) & 0xFF;
          uint8_t lb = resp_data & 0xFF;

          ESP_LOGV(TAG, "[OT_PKT] dir=SlaveToMaster type=intercept msg_id=%d msg_type=%s resp_raw=0x%08lX resp_data=0x%04X hb=0x%02X lb=0x%02X parsed=%.2f valid=1",
                   static_cast<int>(id),
                   instance_->getMessageTypeName(msg_type),
                   response,
                   resp_data,
                   hb, lb,
                   parsed_value);
        }
        else
        {
          ESP_LOGV(TAG, "[OT_PKT] dir=SlaveToMaster type=intercept msg_id=%d msg_type=%s resp_raw=0x%08lX valid=0",
                   static_cast<int>(id),
                   instance_->getMessageTypeName(msg_type),
                   response);
        }

        // Update status response (critical for binary sensors)
        if (id == OpenThermMessageID::Status)
        {
          last_status_response_ = response;
        }

        // Store response for processing in loop() (outside interrupt context)
        if (is_valid)
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
          ESP_LOGV(TAG, "[OT_PKT] type=intercept caching WRITE request for msg_id=%d", static_cast<int>(id));
        }
      }
    }

    void OpenthermComponent::processCachedResponse(unsigned long response, OpenThermMessageID id)
    {
      // This runs in loop(), not interrupt context - safe to do complex operations
      unsigned long now = millis();

      switch (id)
      {
        case OpenThermMessageID::Toutside:
          cached_external_temp_.value = ot_->getFloat(response);
          cached_external_temp_.last_update = now;
          ESP_LOGV(TAG, "Cached external temp: %.1f°C", cached_external_temp_.value);
          break;

        case OpenThermMessageID::Tret:
          cached_return_temp_.value = ot_->getFloat(response);
          cached_return_temp_.last_update = now;
          ESP_LOGV(TAG, "Cached return temp: %.1f°C", cached_return_temp_.value);
          break;

        case OpenThermMessageID::Tboiler:
          cached_boiler_temp_.value = ot_->getFloat(response);
          cached_boiler_temp_.last_update = now;
          ESP_LOGV(TAG, "Cached boiler temp: %.1f°C", cached_boiler_temp_.value);
          break;

        case OpenThermMessageID::CHPressure:
          cached_pressure_.value = ot_->getFloat(response);
          cached_pressure_.last_update = now;
          ESP_LOGV(TAG, "Cached pressure: %.1f bar", cached_pressure_.value);
          break;

        case OpenThermMessageID::RelModLevel:
          cached_modulation_.value = ot_->getFloat(response);
          cached_modulation_.last_update = now;
          ESP_LOGV(TAG, "Cached modulation: %.1f%%", cached_modulation_.value);
          break;

        case OpenThermMessageID::TSet:
          cached_heating_target_.value = ot_->getFloat(response);
          cached_heating_target_.last_update = now;
          ESP_LOGV(TAG, "Cached heating target: %.1f°C", cached_heating_target_.value);
          break;

        case OpenThermMessageID::Tdhw:
          cached_dhw_temp_.value = ot_->getFloat(response);
          cached_dhw_temp_.last_update = now;
          ESP_LOGV(TAG, "Cached DHW temp: %.1f°C", cached_dhw_temp_.value);
          break;

        case OpenThermMessageID::TdhwSet:
          cached_dhw_target_.value = ot_->getFloat(response);
          cached_dhw_target_.last_update = now;
          ESP_LOGV(TAG, "Cached DHW target: %.1f°C", cached_dhw_target_.value);
          break;

        case OpenThermMessageID::Status:
          // Already handled in processRequest for immediate binary sensor updates
          ESP_LOGD(TAG, "Updated status response: %lu", response);
          break;

        default:
          // Other message IDs not cached
          break;
      }
    }

    float OpenthermComponent::getCachedOrFetch(CachedValue &cache, OpenThermMessageID msg_id)
    {
      unsigned long now = millis();

      // Handle first fetch (cache never updated) - last_update will be 0
      if (cache.last_update == 0)
      {
        ESP_LOGV(TAG, "[OT_CACHE] msg_id=%d first_fetch=1", static_cast<int>(msg_id));

        unsigned long request = ot_->buildRequest(OpenThermRequestType::READ, msg_id, 0);
        ESP_LOGV(TAG, "[OT_PKT] dir=GatewayToBoiler type=fetch msg_id=%d msg_type=READ req_raw=0x%08lX",
                 static_cast<int>(msg_id), request);

        unsigned long response = ot_->sendRequest(request);
        bool is_valid = ot_->isValidResponse(response);

        if (is_valid)
        {
          cache.value = ot_->getFloat(response);
          cache.last_update = now;

          uint16_t resp_data = response & 0xFFFF;
          uint8_t hb = (resp_data >> 8) & 0xFF;
          uint8_t lb = resp_data & 0xFF;

          ESP_LOGV(TAG, "[OT_PKT] dir=BoilerToGateway type=fetch msg_id=%d msg_type=READ_ACK resp_raw=0x%08lX resp_data=0x%04X hb=0x%02X lb=0x%02X parsed=%.2f valid=1",
                   static_cast<int>(msg_id), response, resp_data, hb, lb, cache.value);
          return cache.value;
        }
        else
        {
          ESP_LOGV(TAG, "[OT_PKT] dir=BoilerToGateway type=fetch msg_id=%d resp_raw=0x%08lX valid=0",
                   static_cast<int>(msg_id), response);
          ESP_LOGW(TAG, "[OT_CACHE] msg_id=%d first_fetch_failed=1", static_cast<int>(msg_id));
          cache.last_update = now; // Set timestamp to prevent immediate retry
          return NAN;
        }
      }

      // Unsigned arithmetic handles millis() overflow correctly (wraps at 2^32)
      unsigned long cache_age = now - cache.last_update;

      // Check if cache is fresh (updated within last minute)
      if (!std::isnan(cache.value) && cache_age < CACHE_TIMEOUT_)
      {
        ESP_LOGV(TAG, "[OT_CACHE] msg_id=%d cache_hit=1 value=%.2f age_ms=%lu",
                 static_cast<int>(msg_id), cache.value, cache_age);
        return cache.value;
      }

      // Rate limiting: Don't fetch if we just fetched recently (prevents spam if cache keeps expiring)
      if (cache_age < MIN_FETCH_INTERVAL_)
      {
        ESP_LOGV(TAG, "[OT_CACHE] msg_id=%d rate_limited=1 age_ms=%lu min_interval_ms=%lu returning_stale=1",
                 static_cast<int>(msg_id), cache_age, MIN_FETCH_INTERVAL_);
        return cache.value; // Return stale value rather than spam the bus
      }

      // Cache is stale - fetch from boiler
      ESP_LOGV(TAG, "[OT_CACHE] msg_id=%d cache_stale=1 age_ms=%lu fetching=1",
               static_cast<int>(msg_id), cache_age);

      unsigned long request = ot_->buildRequest(OpenThermRequestType::READ, msg_id, 0);
      ESP_LOGV(TAG, "[OT_PKT] dir=GatewayToBoiler type=fetch msg_id=%d msg_type=READ req_raw=0x%08lX",
               static_cast<int>(msg_id), request);

      unsigned long response = ot_->sendRequest(request);
      bool is_valid = ot_->isValidResponse(response);

      if (is_valid)
      {
        cache.value = ot_->getFloat(response);
        cache.last_update = now;

        uint16_t resp_data = response & 0xFFFF;
        uint8_t hb = (resp_data >> 8) & 0xFF;
        uint8_t lb = resp_data & 0xFF;

        ESP_LOGV(TAG, "[OT_PKT] dir=BoilerToGateway type=fetch msg_id=%d msg_type=READ_ACK resp_raw=0x%08lX resp_data=0x%04X hb=0x%02X lb=0x%02X parsed=%.2f valid=1",
                 static_cast<int>(msg_id), response, resp_data, hb, lb, cache.value);
        return cache.value;
      }
      else
      {
        ESP_LOGV(TAG, "[OT_PKT] dir=BoilerToGateway type=fetch msg_id=%d resp_raw=0x%08lX valid=0",
                 static_cast<int>(msg_id), response);
        ESP_LOGW(TAG, "[OT_CACHE] msg_id=%d fetch_failed=1 using_stale=1", static_cast<int>(msg_id));
        // Update timestamp even on failure to prevent continuous retry spam
        cache.last_update = now;
      }

      return cache.value; // Return stale value or NAN
    }

    const char* OpenthermComponent::getMessageTypeName(OpenThermMessageType type)
    {
      switch (type)
      {
        case OpenThermMessageType::READ_DATA: return "READ";
        case OpenThermMessageType::WRITE_DATA: return "WRITE";
        case OpenThermMessageType::INVALID_DATA: return "INVALID";
        case OpenThermMessageType::READ_ACK: return "READ_ACK";
        case OpenThermMessageType::WRITE_ACK: return "WRITE_ACK";
        case OpenThermMessageType::DATA_INVALID: return "DATA_INVALID";
        case OpenThermMessageType::UNKNOWN_DATA_ID: return "UNKNOWN_ID";
        default: return "UNKNOWN";
      }
    }

    const char* OpenthermComponent::getMessageIDName(OpenThermMessageID id)
    {
      switch (id)
      {
        case OpenThermMessageID::Status: return "Status";
        case OpenThermMessageID::TSet: return "TSet";
        case OpenThermMessageID::SConfigSMemberIDcode: return "SlaveConfig";
        case OpenThermMessageID::Command: return "Command";
        case OpenThermMessageID::ASFflags: return "ASFflags";
        case OpenThermMessageID::MaxRelModLevelSetting: return "MaxRelMod";
        case OpenThermMessageID::RelModLevel: return "RelModLevel";
        case OpenThermMessageID::CHPressure: return "CHPressure";
        case OpenThermMessageID::Tboiler: return "Tboiler";
        case OpenThermMessageID::Tdhw: return "Tdhw";
        case OpenThermMessageID::Toutside: return "Toutside";
        case OpenThermMessageID::Tret: return "Tret";
        case OpenThermMessageID::TdhwSet: return "TdhwSet";
        case OpenThermMessageID::MaxTSet: return "MaxTSet";
        case OpenThermMessageID::OEMDiagnosticCode: return "OEMDiag";
        case OpenThermMessageID::OpenThermVersionMaster: return "OT_Ver_Master";
        case OpenThermMessageID::OpenThermVersionSlave: return "OT_Ver_Slave";
        default: return "Unknown";
      }
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
        // Extract full response data
        uint16_t response_data = response & 0xFFFF;
        uint8_t high_byte = (response_data >> 8) & 0xFF;
        uint8_t low_byte = response_data & 0xFF;

        ESP_LOGD(TAG, "BLOR response data: HB=0x%02X (%d), LB=0x%02X (%d)",
                 high_byte, high_byte, low_byte, low_byte);

        // Check if command was accepted (response code in LB should be >= 128 for success)
        // Or check HB for echo of command code
        if (low_byte >= 128 || high_byte == 1)
        {
          ESP_LOGI(TAG, "Boiler reset command completed successfully (HB=%d, LB=%d)", high_byte, low_byte);
          return true;
        }
        else
        {
          ESP_LOGW(TAG, "Boiler reset command failed or not supported (HB=%d, LB=%d)", high_byte, low_byte);
          return false;
        }
      }

      ESP_LOGE(TAG, "Boiler reset command - no valid response");
      return false;
    }

  } // namespace opentherm
} // namespace esphome