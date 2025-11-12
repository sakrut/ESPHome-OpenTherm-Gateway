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

    OpenthermComponent::OpenthermComponent() : PollingComponent(30000)  // 30s polling - cache reduces actual requests
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
    }

    void OpenthermComponent::loop()
    {
      slave_ot_->process();
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

      if (pressure_sensor_ != nullptr && !std::isnan(pressure))
        pressure_sensor_->publish_state(pressure);

      if (modulation_sensor_ != nullptr && !std::isnan(modulation))
        modulation_sensor_->publish_state(modulation);

      if (heating_target_temperature_sensor_ != nullptr && !std::isnan(heating_target_temp))
        heating_target_temperature_sensor_->publish_state(heating_target_temp);

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
        heating_water_climate_->target_temperature = getHeatingTargetTemperature();
        heating_water_climate_->publish_state();
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

    bool OpenthermComponent::setHotWaterTemperature(float temperature)
    {
      ESP_LOGI(TAG, "Setting DHW temperature to %.1f°C", temperature);
      unsigned int data = ot_->temperatureToData(temperature);
      unsigned long request = ot_->buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::TdhwSet, data);
      unsigned long response = ot_->sendRequest(request);

      if (ot_->isValidResponse(response)) {
        // Verify the setpoint was accepted by reading it back
        float actual_setpoint = getHotWaterTargetTemperature();
        if (!std::isnan(actual_setpoint)) {
          ESP_LOGI(TAG, "DHW setpoint verified: %.1f°C (requested: %.1f°C)", actual_setpoint, temperature);

          // Update climate entity immediately with verified value
          if (hot_water_climate_ != nullptr) {
            hot_water_climate_->target_temperature = actual_setpoint;
            hot_water_climate_->publish_state();
          }

          // Check if setpoint was clamped by boiler (e.g., min 35°C)
          if (std::abs(actual_setpoint - temperature) > 1.0f) {
            ESP_LOGW(TAG, "DHW setpoint was adjusted by boiler (min/max limits?)");
          }
        }
        return true;
      }

      ESP_LOGE(TAG, "Failed to set DHW temperature");
      return false;
    }

    bool OpenthermComponent::setHeatingTargetTemperature(float temperature)
    {
      ESP_LOGI(TAG, "Setting CH temperature to %.1f°C", temperature);
      unsigned int data = ot_->temperatureToData(temperature);
      unsigned long request = ot_->buildRequest(OpenThermRequestType::WRITE, OpenThermMessageID::TSet, data);
      unsigned long response = ot_->sendRequest(request);

      if (ot_->isValidResponse(response)) {
        // Verify the setpoint was accepted
        float actual_setpoint = getHeatingTargetTemperature();
        if (!std::isnan(actual_setpoint)) {
          ESP_LOGI(TAG, "CH setpoint verified: %.1f°C (requested: %.1f°C)", actual_setpoint, temperature);

          // Update climate entity immediately
          if (heating_water_climate_ != nullptr) {
            heating_water_climate_->target_temperature = actual_setpoint;
            heating_water_climate_->publish_state();
          }

          if (std::abs(actual_setpoint - temperature) > 1.0f) {
            ESP_LOGW(TAG, "CH setpoint was adjusted by boiler (min/max limits?)");
          }
        }
        return true;
      }

      ESP_LOGE(TAG, "Failed to set CH temperature");
      return false;
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

        // Update status response
        if (id == OpenThermMessageID::Status)
        {
          last_status_response_ = response;
          ESP_LOGD(TAG, "Updated status response: %lu", last_status_response_);
        }

        // Cache values from intercepted requests (only if response is valid)
        if (instance_->ot_->isValidResponse(response))
        {
          unsigned long now = millis();

          switch (id)
          {
            case OpenThermMessageID::Toutside:
              instance_->cached_external_temp_.value = instance_->ot_->getFloat(response);
              instance_->cached_external_temp_.last_update = now;
              ESP_LOGV(TAG, "Cached external temp: %.1f°C", instance_->cached_external_temp_.value);
              break;

            case OpenThermMessageID::Tret:
              instance_->cached_return_temp_.value = instance_->ot_->getFloat(response);
              instance_->cached_return_temp_.last_update = now;
              ESP_LOGV(TAG, "Cached return temp: %.1f°C", instance_->cached_return_temp_.value);
              break;

            case OpenThermMessageID::Tboiler:
              instance_->cached_boiler_temp_.value = instance_->ot_->getFloat(response);
              instance_->cached_boiler_temp_.last_update = now;
              ESP_LOGV(TAG, "Cached boiler temp: %.1f°C", instance_->cached_boiler_temp_.value);
              break;

            case OpenThermMessageID::CHPressure:
              instance_->cached_pressure_.value = instance_->ot_->getFloat(response);
              instance_->cached_pressure_.last_update = now;
              ESP_LOGV(TAG, "Cached pressure: %.1f bar", instance_->cached_pressure_.value);
              break;

            case OpenThermMessageID::RelModLevel:
              instance_->cached_modulation_.value = instance_->ot_->getFloat(response);
              instance_->cached_modulation_.last_update = now;
              ESP_LOGV(TAG, "Cached modulation: %.1f%%", instance_->cached_modulation_.value);
              break;

            case OpenThermMessageID::TSet:
              instance_->cached_heating_target_.value = instance_->ot_->getFloat(response);
              instance_->cached_heating_target_.last_update = now;
              ESP_LOGV(TAG, "Cached heating target: %.1f°C", instance_->cached_heating_target_.value);
              break;

            case OpenThermMessageID::Tdhw:
              instance_->cached_dhw_temp_.value = instance_->ot_->getFloat(response);
              instance_->cached_dhw_temp_.last_update = now;
              ESP_LOGV(TAG, "Cached DHW temp: %.1f°C", instance_->cached_dhw_temp_.value);
              break;

            case OpenThermMessageID::TdhwSet:
              instance_->cached_dhw_target_.value = instance_->ot_->getFloat(response);
              instance_->cached_dhw_target_.last_update = now;
              ESP_LOGV(TAG, "Cached DHW target: %.1f°C", instance_->cached_dhw_target_.value);
              break;

            default:
              // Other message IDs not cached
              break;
          }
        }
      }
    }

    float OpenthermComponent::getCachedOrFetch(CachedValue &cache, OpenThermMessageID msg_id)
    {
      unsigned long now = millis();

      // Check if cache is fresh (updated within last minute)
      if (!std::isnan(cache.value) && (now - cache.last_update) < CACHE_TIMEOUT_)
      {
        ESP_LOGV(TAG, "Using cached value for msg_id %d: %.2f (age: %lu ms)",
                 static_cast<int>(msg_id), cache.value, now - cache.last_update);
        return cache.value;
      }

      // Cache is stale or empty - fetch from boiler
      ESP_LOGV(TAG, "Cache stale/empty for msg_id %d, fetching from boiler", static_cast<int>(msg_id));
      unsigned long response = ot_->sendRequest(ot_->buildRequest(OpenThermRequestType::READ, msg_id, 0));

      if (ot_->isValidResponse(response))
      {
        cache.value = ot_->getFloat(response);
        cache.last_update = now;
        return cache.value;
      }

      return NAN;
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

      unsigned long response = ot_->sendRequest(request);

      if (ot_->isValidResponse(response))
      {
        // Extract command response code from low byte
        uint8_t cmd_response = response & 0xFF;

        if (cmd_response >= 128)
        {
          ESP_LOGI(TAG, "Boiler reset command completed successfully (response: %d)", cmd_response);
          return true;
        }
        else
        {
          ESP_LOGW(TAG, "Boiler reset command failed (response: %d)", cmd_response);
          return false;
        }
      }

      ESP_LOGE(TAG, "Boiler reset command - no valid response");
      return false;
    }

  } // namespace opentherm
} // namespace esphome