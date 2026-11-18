#pragma once

namespace esphome
{
  namespace binary_sensor
  {
    class BinarySensor
    {
    public:
      bool state{false};
      void publish_state(bool value) { state = value; }
    };
  } // namespace binary_sensor
} // namespace esphome
