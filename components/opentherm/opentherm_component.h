#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "OpenTherm.h"
#include "opentherm_climate.h"
#include "opentherm_number.h"
#include <map>
#include <vector>

namespace esphome
{
  namespace opentherm
  {

    enum class ClimateType
    {
      HOT_WATER,
      HEATING_WATER,
      ROOM
    };

    // Define constants in the namespace for easier access
    constexpr auto HOT_WATER = ClimateType::HOT_WATER;
    constexpr auto HEATING_WATER = ClimateType::HEATING_WATER;
    constexpr auto ROOM = ClimateType::ROOM;

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
      void set_exhaust_temperature_sensor(sensor::Sensor *sensor) { exhaust_temperature_sensor_ = sensor; }
      void set_pressure_sensor(sensor::Sensor *sensor) { pressure_sensor_ = sensor; }
      void set_modulation_sensor(sensor::Sensor *sensor) { modulation_sensor_ = sensor; }
      void set_heating_target_temperature_sensor(sensor::Sensor *sensor) { heating_target_temperature_sensor_ = sensor; }
      void set_room_temperature_sensor(sensor::Sensor *sensor) { room_temperature_sensor_ = sensor; }
      void set_room_setpoint_sensor(sensor::Sensor *sensor) { room_setpoint_sensor_ = sensor; }
      void set_room_climate_external_sensor(sensor::Sensor *sensor) { room_climate_external_sensor_ = sensor; }
      void add_read_sensor(sensor::Sensor *sensor, uint8_t message_id);

      // Phase 1 sensor setters
      void set_max_ch_setpoint_sensor(sensor::Sensor *sensor) { max_ch_setpoint_sensor_ = sensor; }
      void set_min_ch_setpoint_sensor(sensor::Sensor *sensor) { min_ch_setpoint_sensor_ = sensor; }
      void set_max_modulation_sensor(sensor::Sensor *sensor) { max_modulation_sensor_ = sensor; }
      void set_oem_fault_code_sensor(sensor::Sensor *sensor) { oem_fault_code_sensor_ = sensor; }
      void set_oem_diagnostic_code_sensor(sensor::Sensor *sensor) { oem_diagnostic_code_sensor_ = sensor; }
      void set_master_ot_version_sensor(sensor::Sensor *sensor) { master_ot_version_sensor_ = sensor; }
      void set_slave_ot_version_sensor(sensor::Sensor *sensor) { slave_ot_version_sensor_ = sensor; }
      void set_dropped_frames_sensor(sensor::Sensor *sensor) { dropped_frames_sensor_ = sensor; }
      void set_rejected_frames_sensor(sensor::Sensor *sensor) { rejected_frames_sensor_ = sensor; }

      // Frame validation
      void set_check_spare_bits(bool check) { check_spare_bits_ = check; }
      void set_check_response_id(bool check) { check_response_id_ = check; }
      void set_write_confirmations(uint8_t confirmations) { write_confirmations_ = confirmations; }
      void add_tsp(uint8_t index, bool is_signed, int min_value, int max_value, uint32_t poll_interval,
                   sensor::Sensor *sensor, OpenthermNumber *number);

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
      float getRoomSetpoint();
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
      sensor::Sensor *exhaust_temperature_sensor_{nullptr};
      sensor::Sensor *pressure_sensor_{nullptr};
      sensor::Sensor *modulation_sensor_{nullptr};
      sensor::Sensor *heating_target_temperature_sensor_{nullptr};
      sensor::Sensor *room_temperature_sensor_{nullptr};
      sensor::Sensor *room_setpoint_sensor_{nullptr};
      sensor::Sensor *room_climate_external_sensor_{nullptr};

      // Phase 1 sensors
      sensor::Sensor *max_ch_setpoint_sensor_{nullptr};
      sensor::Sensor *min_ch_setpoint_sensor_{nullptr};
      sensor::Sensor *max_modulation_sensor_{nullptr};
      sensor::Sensor *oem_fault_code_sensor_{nullptr};
      sensor::Sensor *oem_diagnostic_code_sensor_{nullptr};
      sensor::Sensor *master_ot_version_sensor_{nullptr};
      sensor::Sensor *slave_ot_version_sensor_{nullptr};
      sensor::Sensor *dropped_frames_sensor_{nullptr};
      sensor::Sensor *rejected_frames_sensor_{nullptr};

      // Binary Sensors
      binary_sensor::BinarySensor *flame_{nullptr};
      binary_sensor::BinarySensor *ch_active_{nullptr};
      binary_sensor::BinarySensor *dhw_active_{nullptr};
      binary_sensor::BinarySensor *fault_{nullptr};
      binary_sensor::BinarySensor *diagnostic_{nullptr};

      // Climate controllers
      OpenthermClimate *hot_water_climate_{nullptr};
      OpenthermClimate *heating_water_climate_{nullptr};
      OpenthermClimate *room_climate_{nullptr};

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

      enum class ValueEncoding {
        F88,
        U16,
        S16,
        RAW,
      };

      struct CachedMessageSpec {
        OpenThermMessageID id;
        CachedValue OpenthermComponent::*cache;
        const char *log_name;
      };

      struct GenericReadSensor {
        sensor::Sensor *sensor{nullptr};
        OpenThermMessageID id{static_cast<OpenThermMessageID>(0)};
        CachedValue cache;
      };

      CachedValue cached_external_temp_;
      CachedValue cached_return_temp_;
      CachedValue cached_boiler_temp_;
      CachedValue cached_exhaust_temp_;
      CachedValue cached_pressure_;
      CachedValue cached_modulation_;
      CachedValue cached_heating_target_;
      CachedValue cached_dhw_temp_;
      CachedValue cached_dhw_target_;
      CachedValue cached_room_temp_;
      CachedValue cached_room_setpoint_;
      std::vector<GenericReadSensor> read_sensors_;

      // Gateway's own boiler reads, sent one at a time from loop() between thermostat frames
      struct PendingFetch {
        CachedValue *cache;
        OpenThermMessageID id;
        uint16_t data;
      };
      std::vector<PendingFetch> pending_fetches_;
      unsigned long last_relay_ms_{0};

      // Thermostat frames not forwarded because the library reported them INVALID or TIMEOUT
      uint32_t dropped_frames_{0};
      // Frames with valid parity rejected by checkThermostatWrite/isValidReply/spare bits
      uint32_t rejected_frames_{0};
      bool check_spare_bits_{true};
      bool check_response_id_{true};
      uint8_t write_confirmations_{2};
      // Last WRITE data passed to the boiler, keyed by data-id << 8 (| TSP index for data-id 11)
      std::map<uint16_t, uint16_t> thermostat_writes_;
      struct PendingWrite {
        uint16_t data;
        uint8_t count;
        unsigned long since;
      };
      std::map<uint16_t, PendingWrite> pending_writes_;

      // Manufacturer-specific transparent slave parameters (Data-ID 11), configured in YAML
      struct TspParam {
        uint8_t index;
        bool is_signed;
        int min_value;
        int max_value;
        uint32_t poll_interval;
        sensor::Sensor *sensor;
        OpenthermNumber *number;
        CachedValue cache;  // raw 16-bit READ_ACK data
        unsigned long checked_at{0};
      };
      std::vector<TspParam> tsp_params_;

      const unsigned long CACHE_TIMEOUT_{60000};  // 1 minute in ms
      const unsigned long MIN_FETCH_INTERVAL_{5000};  // Minimum 5s between fetch requests for same sensor
      const unsigned long FETCH_WINDOW_{200};  // Own reads start at most 200ms after a relayed thermostat frame
      const unsigned long THERMOSTAT_SILENT_{3000};  // No thermostat traffic for 3s: read without waiting
      const unsigned long CONFIRM_WINDOW_{30000};  // Repeats of a held WRITE must arrive within 30s

      // Returns cached value; queues a boiler read if stale (sent later by fetchPendingValue)
      float getCachedOrFetch(CachedValue &cache, OpenThermMessageID msg_id);
      void queueFetch(CachedValue &cache, OpenThermMessageID msg_id, uint16_t data);
      void fetchPendingValue();
      bool checkThermostatWrite(unsigned long request);
      bool isValidReply(unsigned long request, unsigned long response) const;
      void rejectFrame(unsigned long frame, const char *reason);
      int tspValue(const TspParam &tsp, uint16_t data) const;
      void checkTsp(TspParam &tsp);
      bool writeTsp(TspParam &tsp, int value);
      float readGenericSensorValue(GenericReadSensor &read_sensor);
      float decodeMessageValue(unsigned long response, OpenThermMessageID id);
      ValueEncoding valueEncodingForMessage(OpenThermMessageID id) const;
      const CachedMessageSpec *cachedMessageSpec(OpenThermMessageID id) const;

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
