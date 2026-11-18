#pragma once

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <optional>

namespace esphome
{
  namespace climate
  {
    enum ClimateMode
    {
      CLIMATE_MODE_OFF,
      CLIMATE_MODE_HEAT
    };

    enum ClimateAction
    {
      CLIMATE_ACTION_OFF,
      CLIMATE_ACTION_HEATING
    };

    enum ClimateFeature : uint32_t
    {
      CLIMATE_SUPPORTS_CURRENT_TEMPERATURE = 1,
      CLIMATE_SUPPORTS_ACTION = 2
    };

    class ClimateTraits
    {
    public:
      void add_feature_flags(uint32_t) {}
      void set_supported_modes(std::initializer_list<ClimateMode>) {}
      void set_visual_min_temperature(float) {}
      void set_visual_max_temperature(float) {}
      void set_visual_temperature_step(float) {}
    };

    class ClimateCall
    {
    public:
      std::optional<ClimateMode> mode;
      std::optional<float> target_temperature;
      const std::optional<ClimateMode> &get_mode() const { return mode; }
      const std::optional<float> &get_target_temperature() const { return target_temperature; }
    };

    class Climate
    {
    public:
      virtual ~Climate() = default;
      ClimateMode mode{CLIMATE_MODE_OFF};
      ClimateAction action{CLIMATE_ACTION_OFF};
      float target_temperature{NAN};
      float current_temperature{NAN};
      void publish_state() {}
      virtual ClimateTraits traits() = 0;
      virtual void control(const ClimateCall &call) = 0;
    };
  } // namespace climate
} // namespace esphome
