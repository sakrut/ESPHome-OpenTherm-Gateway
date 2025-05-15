#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include <functional>

namespace esphome
{
  namespace opentherm
  {

    enum class ClimateType;

    class OpenthermClimate : public climate::Climate, public Component
    {
    public:
      void setup() override;
      void dump_config() override;

      climate::ClimateTraits traits() override;
      void control(const climate::ClimateCall &call) override;

      void set_target_temperature_callback(std::function<bool(float)> callback) { target_temperature_setter_ = callback; }
      void set_climate_type(ClimateType type) { climate_type_ = type; }
      ClimateType get_climate_type() const { return climate_type_; }

    protected:
      float default_target_temperature_{0};
      std::function<bool(float)> target_temperature_setter_;
      ClimateType climate_type_;
    };

  } // namespace opentherm
} // namespace esphome
