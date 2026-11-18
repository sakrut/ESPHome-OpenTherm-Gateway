#pragma once

#include <cmath>

namespace esphome
{
  namespace sensor
  {
    class Sensor
    {
    public:
      float state{NAN};
      bool has_state() const { return has_state_; }
      void publish_state(float value)
      {
        state = value;
        has_state_ = true;
      }

    private:
      bool has_state_{false};
    };
  } // namespace sensor
} // namespace esphome
