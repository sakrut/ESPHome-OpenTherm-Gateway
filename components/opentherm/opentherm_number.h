#pragma once

#include "esphome/components/number/number.h"
#include <functional>

namespace esphome
{
  namespace opentherm
  {

    /// Number entity that forwards HA writes to the gateway (TSP control).
    class OpenthermNumber : public number::Number
    {
    public:
      void set_control_callback(std::function<void(float)> callback) { control_callback_ = std::move(callback); }

    protected:
      void control(float value) override
      {
        if (control_callback_)
          control_callback_(value);
      }

      std::function<void(float)> control_callback_;
    };

  } // namespace opentherm
} // namespace esphome
