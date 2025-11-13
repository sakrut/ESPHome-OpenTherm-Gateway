#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "OpenTherm.h"
#include "opentherm_climate.h"

namespace esphome
{
  namespace opentherm
  {

    enum class ClimateType
    {
      HOT_WATER,
      HEATING_WATER
    };

    // Define constants in the namespace for easier access
    constexpr auto HOT_WATER = ClimateType::HOT_WATER;
    constexpr auto HEATING_WATER = ClimateType::HEATING_WATER;

    class OpenthermClimate;

    class OpenthermComponent : public PollingComponent
    {
    public:
      OpenthermComponent(uint32_t update_interval);

      void setup() override;
      void loop() override;
      void update() override;

      // Pin configurations
      void set_in_pin(int pin) { in_pin_ = pin; }
      void set_out_pin(int pin) { out_pin_ = pin; }
      void set_slave_in_pin(int pin) { slave_in_pin_ = pin; }
      void set_slave_out_pin(int pin) { slave_out_pin_ = pin; }

      // Sensor setters
      void set_external_temperature_sensor(sensor::Sensor *sensor) { external_temperature_sensor_ = sensor; }
      void set_return_temperature_sensor(sensor::Sensor *sensor) { return_temperature_sensor_ = sensor; }
      void set_boiler_temperature_sensor(sensor::Sensor *sensor) { boiler_temperature_ = sensor; }
      void set_pressure_sensor(sensor::Sensor *sensor) { pressure_sensor_ = sensor; }
      void set_modulation_sensor(sensor::Sensor *sensor) { modulation_sensor_ = sensor; }
      void set_heating_target_temperature_sensor(sensor::Sensor *sensor) { heating_target_temperature_sensor_ = sensor; }

      // Phase 1 sensor setters
      void set_max_ch_setpoint_sensor(sensor::Sensor *sensor) { max_ch_setpoint_sensor_ = sensor; }
      void set_min_ch_setpoint_sensor(sensor::Sensor *sensor) { min_ch_setpoint_sensor_ = sensor; }
      void set_max_modulation_sensor(sensor::Sensor *sensor) { max_modulation_sensor_ = sensor; }
      void set_oem_fault_code_sensor(sensor::Sensor *sensor) { oem_fault_code_sensor_ = sensor; }
      void set_oem_diagnostic_code_sensor(sensor::Sensor *sensor) { oem_diagnostic_code_sensor_ = sensor; }
      void set_master_ot_version_sensor(sensor::Sensor *sensor) { master_ot_version_sensor_ = sensor; }
      void set_slave_ot_version_sensor(sensor::Sensor *sensor) { slave_ot_version_sensor_ = sensor; }

      // Binary sensor setters
      void set_flame_sensor(binary_sensor::BinarySensor *sensor) { flame_ = sensor; }
      void set_ch_active_sensor(binary_sensor::BinarySensor *sensor) { ch_active_ = sensor; }
      void set_dhw_active_sensor(binary_sensor::BinarySensor *sensor) { dhw_active_ = sensor; }
      void set_fault_sensor(binary_sensor::BinarySensor *sensor) { fault_ = sensor; }
      void set_diagnostic_sensor(binary_sensor::BinarySensor *sensor) { diagnostic_ = sensor; }

      // Climate registration
      void register_climate(OpenthermClimate *climate);

      // OpenTherm functions
      float getExternalTemperature();
      float getHeatingTargetTemperature();
      float getReturnTemperature();
      float getHotWaterTargetTemperature();
      float getHotWaterTemperature();
      float getRoomTemperature();
      bool setHotWaterTemperature(float temperature);
      bool setHeatingTargetTemperature(float temperature);
      float getModulation();
      float getPressure();

      // Boiler lockout reset (BLOR command)
      bool sendBoilerReset();

      // Process OpenTherm requests - needs to be static for the interrupt handler
      static void processRequest(unsigned long request, OpenThermResponseStatus status);

    protected:
      static OpenthermComponent *instance_;

      // Pin configurations
      int in_pin_{4};
      int out_pin_{5};
      int slave_in_pin_{12};
      int slave_out_pin_{13};

      // OpenTherm instances
      OpenTherm *ot_{nullptr};
      OpenTherm *slave_ot_{nullptr};

      // Sensors
      sensor::Sensor *external_temperature_sensor_{nullptr};
      sensor::Sensor *return_temperature_sensor_{nullptr};
      sensor::Sensor *boiler_temperature_{nullptr};
      sensor::Sensor *pressure_sensor_{nullptr};
      sensor::Sensor *modulation_sensor_{nullptr};
      sensor::Sensor *heating_target_temperature_sensor_{nullptr};

      // Phase 1 sensors
      sensor::Sensor *max_ch_setpoint_sensor_{nullptr};
      sensor::Sensor *min_ch_setpoint_sensor_{nullptr};
      sensor::Sensor *max_modulation_sensor_{nullptr};
      sensor::Sensor *oem_fault_code_sensor_{nullptr};
      sensor::Sensor *oem_diagnostic_code_sensor_{nullptr};
      sensor::Sensor *master_ot_version_sensor_{nullptr};
      sensor::Sensor *slave_ot_version_sensor_{nullptr};

      // Binary Sensors
      binary_sensor::BinarySensor *flame_{nullptr};
      binary_sensor::BinarySensor *ch_active_{nullptr};
      binary_sensor::BinarySensor *dhw_active_{nullptr};
      binary_sensor::BinarySensor *fault_{nullptr};
      binary_sensor::BinarySensor *diagnostic_{nullptr};

      // Climate controllers
      OpenthermClimate *hot_water_climate_{nullptr};
      OpenthermClimate *heating_water_climate_{nullptr};

      // Last status response
      static unsigned long last_status_response_;

      // Last intercepted response (set by processRequest, processed in loop)
      static unsigned long last_intercepted_response_;
      static OpenThermMessageID last_intercepted_id_;
      static bool has_new_intercepted_response_;

      // Cached sensor values with timestamps (value updated by processRequest or explicit poll)
      struct CachedValue {
        float value{NAN};
        unsigned long last_update{0};
      };

      CachedValue cached_external_temp_;
      CachedValue cached_return_temp_;
      CachedValue cached_boiler_temp_;
      CachedValue cached_pressure_;
      CachedValue cached_modulation_;
      CachedValue cached_heating_target_;
      CachedValue cached_dhw_temp_;
      CachedValue cached_dhw_target_;

      const unsigned long CACHE_TIMEOUT_{60000};  // 1 minute in ms
      const unsigned long MIN_FETCH_INTERVAL_{5000};  // Minimum 5s between fetch requests for same sensor

      // Helper to get cached value or fetch if stale
      float getCachedOrFetch(CachedValue &cache, OpenThermMessageID msg_id);

      // Process intercepted response (called from loop, not interrupt)
      void processCachedResponse(unsigned long response, OpenThermMessageID id);

      // Helper for temperature setpoint verification with retry logic
      bool setTemperatureWithVerification(
          float temperature,
          OpenThermMessageID write_msg_id,
          OpenThermMessageID read_msg_id,
          OpenthermClimate *climate,
          const char *name);

      // Interrupt handlers
      static void IRAM_ATTR handleInterrupt();
      static void IRAM_ATTR slaveHandleInterrupt();
    };

  } // namespace opentherm
} // namespace esphome