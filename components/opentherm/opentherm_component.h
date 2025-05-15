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
      OpenthermComponent();

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

      // Interrupt handlers
      static void IRAM_ATTR handleInterrupt();
      static void IRAM_ATTR slaveHandleInterrupt();
    };

  } // namespace opentherm
} // namespace esphome