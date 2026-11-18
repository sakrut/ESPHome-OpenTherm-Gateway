#include "opentherm_component.h"
#include "esphome/core/log.h"

namespace esphome
{
  namespace opentherm
  {

    static const char *const TAG = "opentherm.component";
    static const unsigned long SPARE_BITS = 0x0F000000;

    static bool isProtectedWrite(uint8_t id)
    {
      switch (id)
      {
        case 1:  // TSet
        case 4:  // Command
        case 11: // TSP
        case 13: // FHB
        case 14: // MaxRelModLevelSetting
        case 56: // TdhwSet
        case 57: // MaxTSet
          return true;
        default:
          return id >= 128; // manufacturer-specific
      }
    }

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

    void OpenthermComponent::add_tsp(uint8_t index, bool is_signed, int min_value, int max_value,
                                     uint32_t poll_interval, sensor::Sensor *sensor, OpenthermNumber *number)
    {
      TspParam tsp;
      tsp.index = index;
      tsp.is_signed = is_signed;
      tsp.min_value = min_value;
      tsp.max_value = max_value;
      tsp.poll_interval = poll_interval;
      tsp.sensor = sensor;
      tsp.number = number;
      tsp_params_.push_back(tsp);
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

      // tsp_params_ is complete here, so references into it stay valid
      for (auto &tsp : tsp_params_)
      {
        if (tsp.number != nullptr)
          tsp.number->set_control_callback([this, &tsp](float value)
                                           { this->writeTsp(tsp, static_cast<int>(lroundf(value))); });
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

      fetchPendingValue();
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

      if (dropped_frames_sensor_ != nullptr)
        dropped_frames_sensor_->publish_state(dropped_frames_);

      if (rejected_frames_sensor_ != nullptr)
        rejected_frames_sensor_->publish_state(rejected_frames_);

      for (auto &tsp : tsp_params_)
      {
        if (tsp.cache.last_update != tsp.checked_at)
        {
          tsp.checked_at = tsp.cache.last_update;
          checkTsp(tsp);
        }
        if (tsp.cache.last_update == 0 || millis() - tsp.cache.last_update >= tsp.poll_interval)
        {
          tsp.cache.value = NAN; // a failed read stays NAN, so checkTsp never acts on a stale value
          queueFetch(tsp.cache, OpenThermMessageID::TSPindexTSPvalue, tsp.index << 8);
        }
      }

      // Temperature and other sensors (using cache with timeout).
      // Only values something consumes are read, so unsupported IDs don't cost bus time.
      float ext_temperature = external_temperature_sensor_ != nullptr ? getExternalTemperature() : NAN;
      float return_temperature = return_temperature_sensor_ != nullptr ? getReturnTemperature() : NAN;
      float boiler_temperature = (boiler_temperature_ != nullptr || heating_water_climate_ != nullptr)
                                     ? getCachedOrFetch(cached_boiler_temp_, OpenThermMessageID::Tboiler)
                                     : NAN;
      float exhaust_temperature = exhaust_temperature_sensor_ != nullptr
                                      ? getCachedOrFetch(cached_exhaust_temp_, OpenThermMessageID::Texhaust)
                                      : NAN;
      float pressure = pressure_sensor_ != nullptr ? getPressure() : NAN;
      float modulation = modulation_sensor_ != nullptr ? getModulation() : NAN;
      float heating_target_temp = (heating_target_temperature_sensor_ != nullptr || heating_water_climate_ != nullptr)
                                      ? getHeatingTargetTemperature()
                                      : NAN;
      float hot_water_temp = hot_water_climate_ != nullptr ? getHotWaterTemperature() : NAN;

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
        float hot_water_target = getHotWaterTargetTemperature();
        if (!std::isnan(hot_water_target))
          hot_water_climate_->target_temperature = hot_water_target;
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
      if (instance_ == nullptr || instance_->ot_ == nullptr || instance_->slave_ot_ == nullptr)
        return;

      // The library also calls back for partial (INVALID) and stale (TIMEOUT) frames.
      // Forwarding those sends random type/id/data to the boiler, so drop them.
      if (status != OpenThermResponseStatus::SUCCESS)
      {
        instance_->dropped_frames_++;
        ESP_LOGW(TAG, "Dropped thermostat frame 0x%08lX (%s), total dropped: %u",
                 request, instance_->slave_ot_->statusToString(status), static_cast<unsigned>(instance_->dropped_frames_));
        return;
      }

      // Parity passes for about half of the corrupted frames, so check the fields the spec fixes
      if (instance_->check_spare_bits_ && (request & SPARE_BITS))
      {
        instance_->rejectFrame(request, "spare bits set");
        return;
      }

      OpenThermMessageID id = instance_->ot_->getDataID(request);
      OpenThermMessageType msg_type = instance_->ot_->getMessageType(request);

      if (msg_type == OpenThermMessageType::WRITE_DATA && !instance_->checkThermostatWrite(request))
        return;

      unsigned long response = instance_->ot_->sendRequest(request);
      bool relayed = instance_->isValidReply(request, response);
      // Not answering makes the thermostat time out and retry instead of acting on garbage
      if (relayed)
        instance_->slave_ot_->sendResponse(response);
      else if (response != 0)
        instance_->rejectFrame(response, "boiler response does not match request");
      instance_->last_relay_ms_ = millis();

      bool valid = relayed && instance_->ot_->isValidResponse(response);

      ESP_LOGV(TAG, "Relayed 0x%08lX -> 0x%08lX", request, response);

      // Update status response (critical for binary sensors)
      if (id == OpenThermMessageID::Status && valid)
      {
        last_status_response_ = response;
      }

      // Store response for processing in loop()
      if (valid)
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
      }
    }

    bool OpenthermComponent::checkThermostatWrite(unsigned long request)
    {
      uint8_t id = (request >> 16) & 0xFF;
      uint16_t data = request & 0xFFFF;
      bool is_tsp = id == static_cast<uint8_t>(OpenThermMessageID::TSPindexTSPvalue);

      uint16_t key = (id << 8) | (is_tsp ? data >> 8 : 0);
      auto pending = pending_writes_.find(key);
      if (pending != pending_writes_.end() && pending->second.data != data)
      {
        rejected_frames_++;
        ESP_LOGW(TAG, "Discarded unconfirmed thermostat WRITE data-id %u: 0x%04X", id, pending->second.data);
        pending_writes_.erase(pending);
        pending = pending_writes_.end();
      }

      auto known = thermostat_writes_.find(key);
      bool unchanged = known != thermostat_writes_.end() && known->second == data;
      if (!unchanged && write_confirmations_ > 1 && isProtectedWrite(id))
      {
        if (pending == pending_writes_.end())
        {
          // shortcut: a corrupt frame whose key never repeats stays pending; the cap bounds memory
          if (pending_writes_.size() >= 8)
            pending_writes_.erase(pending_writes_.begin());
          pending = pending_writes_.emplace(key, PendingWrite{data, 0}).first;
        }
        if (++pending->second.count < write_confirmations_)
        {
          ESP_LOGI(TAG, "Holding thermostat WRITE data-id %u: 0x%04X until repeated (%u/%u)", id, data,
                   pending->second.count, write_confirmations_);
          return false;
        }
        pending_writes_.erase(pending);
      }

      if (known == thermostat_writes_.end())
        // Expected once per key after boot; later, a new key points to a corrupted frame
        ESP_LOGW(TAG, "Thermostat WRITE to new data-id %u: 0x%04X", id, data);
      else if (!unchanged)
        ESP_LOGD(TAG, "Thermostat WRITE data-id %u: 0x%04X -> 0x%04X", id, known->second, data);
      thermostat_writes_[key] = data;
      return true;
    }

    bool OpenthermComponent::isValidReply(unsigned long request, unsigned long response) const
    {
      if (response == 0 || ot_->parity(response))
        return false;
      OpenThermMessageType type = ot_->getMessageType(response);
      if (static_cast<uint8_t>(type) < 4) // responses: READ_ACK, WRITE_ACK, DATA_INVALID, UNKNOWN_DATA_ID
        return false;
      if (check_spare_bits_ && (response & SPARE_BITS))
        return false;
      if (!check_response_id_)
        return true;
      if (ot_->getDataID(response) != ot_->getDataID(request))
        return false;
      // A TSP READ_ACK echoes the requested index
      return ot_->getDataID(request) != OpenThermMessageID::TSPindexTSPvalue ||
             type != OpenThermMessageType::READ_ACK || ((response ^ request) & 0xFF00) == 0;
    }

    void OpenthermComponent::rejectFrame(unsigned long frame, const char *reason)
    {
      rejected_frames_++;
      ESP_LOGW(TAG, "Rejected frame 0x%08lX (%s), total rejected: %u", frame, reason,
               static_cast<unsigned>(rejected_frames_));
    }

    OpenthermComponent::TspParam *OpenthermComponent::findTsp(uint8_t index)
    {
      for (auto &tsp : tsp_params_)
      {
        if (tsp.index == index)
          return &tsp;
      }
      return nullptr;
    }

    int OpenthermComponent::tspValue(const TspParam &tsp, uint16_t data) const
    {
      uint8_t value = data & 0xFF;
      return tsp.is_signed ? static_cast<int8_t>(value) : value;
    }

    void OpenthermComponent::checkTsp(TspParam &tsp)
    {
      if (std::isnan(tsp.cache.value))
        return;

      int value = tspValue(tsp, static_cast<uint16_t>(tsp.cache.value));
      if (tsp.sensor != nullptr)
        tsp.sensor->publish_state(value);

      if (value < tsp.min_value || value > tsp.max_value)
        ESP_LOGW(TAG, "TSP 0x%02X = %d outside [%d, %d]", tsp.index, value, tsp.min_value, tsp.max_value);
      else if (tsp.number != nullptr)
        tsp.number->publish_state(value);
    }

    bool OpenthermComponent::writeTsp(TspParam &tsp, int value)
    {
      if (value < tsp.min_value || value > tsp.max_value)
      {
        ESP_LOGW(TAG, "TSP 0x%02X: refusing to write %d outside [%d, %d]", tsp.index, value, tsp.min_value,
                 tsp.max_value);
        return false;
      }

      ESP_LOGI(TAG, "Writing TSP 0x%02X = %d", tsp.index, value);
      unsigned long request = ot_->buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::TSPindexTSPvalue,
                                                (tsp.index << 8) | static_cast<uint8_t>(value));
      unsigned long response = ot_->sendRequest(request);
      slave_ot_->process();
      if (!isValidReply(request, response) || ot_->getMessageType(response) != OpenThermMessageType::WRITE_ACK)
      {
        ESP_LOGE(TAG, "TSP 0x%02X write failed (response 0x%08lX)", tsp.index, response);
        return false;
      }

      request = ot_->buildRequest(OpenThermRequestType::READ, OpenThermMessageID::TSPindexTSPvalue, tsp.index << 8);
      response = ot_->sendRequest(request);
      slave_ot_->process();
      if (!isValidReply(request, response) || ot_->getMessageType(response) != OpenThermMessageType::READ_ACK)
      {
        ESP_LOGW(TAG, "TSP 0x%02X write acknowledged, read-back failed (response 0x%08lX)", tsp.index, response);
        return false;
      }

      int actual = tspValue(tsp, response & 0xFFFF);
      tsp.cache.value = response & 0xFFFF;
      tsp.cache.last_update = millis();
      tsp.checked_at = tsp.cache.last_update;
      if (tsp.sensor != nullptr)
        tsp.sensor->publish_state(actual);
      if (actual != value)
      {
        ESP_LOGW(TAG, "TSP 0x%02X read back %d after writing %d", tsp.index, actual, value);
        return false;
      }
      if (tsp.number != nullptr)
        tsp.number->publish_state(actual);
      ESP_LOGI(TAG, "TSP 0x%02X verified: %d", tsp.index, actual);
      return true;
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
      // Unsigned arithmetic handles millis() overflow correctly (wraps at 2^32)
      unsigned long age = millis() - cache.last_update;
      bool never_read = cache.last_update == 0;
      bool fresh = !never_read && !std::isnan(cache.value) && age < CACHE_TIMEOUT_;
      // Rate limit: a failed read also sets last_update, so unsupported IDs aren't retried every update
      bool recently_tried = !never_read && age < MIN_FETCH_INTERVAL_;

      if (!fresh && !recently_tried)
        queueFetch(cache, msg_id, 0);

      return cache.value; // Fresh, stale or NAN; a queued read updates it for the next update()
    }

    void OpenthermComponent::queueFetch(CachedValue &cache, OpenThermMessageID msg_id, uint16_t data)
    {
      for (const auto &pending : pending_fetches_)
      {
        if (pending.cache == &cache)
          return;
      }
      pending_fetches_.push_back({&cache, msg_id, data});
    }

    void OpenthermComponent::fetchPendingValue()
    {
      if (pending_fetches_.empty() || !slave_ot_->isReady())
        return;

      // The thermostat sends about one frame per second. Reading right after a relayed frame keeps our
      // blocking request out of the way of the next one; with a silent thermostat, read anyway.
      unsigned long since_relay = millis() - last_relay_ms_;
      if (since_relay > FETCH_WINDOW_ && since_relay < THERMOSTAT_SILENT_)
        return;

      // Drop entries an intercepted thermostat exchange already refreshed since they were queued
      while (!pending_fetches_.empty())
      {
        const CachedValue *cache = pending_fetches_.front().cache;
        if (std::isnan(cache->value) || millis() - cache->last_update >= CACHE_TIMEOUT_)
          break;
        pending_fetches_.erase(pending_fetches_.begin());
      }
      if (pending_fetches_.empty())
        return;

      PendingFetch fetch = pending_fetches_.front();
      pending_fetches_.erase(pending_fetches_.begin());

      unsigned long request = ot_->buildRequest(OpenThermRequestType::READ, fetch.id, fetch.data);
      unsigned long response = ot_->sendRequest(request);
      fetch.cache->last_update = millis(); // Also on failure, to rate limit retries

      if (isValidReply(request, response) && ot_->isValidResponse(response))
      {
        fetch.cache->value = decodeMessageValue(response, fetch.id);
        ESP_LOGV(TAG, "Fetched msg_id %d: %.2f", static_cast<int>(fetch.id), fetch.cache->value);
      }
      else
      {
        ESP_LOGW(TAG, "Failed to fetch value for msg_id %d (response 0x%08lX), using stale cache if available",
                 static_cast<int>(fetch.id), response);
      }

      // Forward a thermostat frame that arrived while we were blocked
      slave_ot_->process();
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
