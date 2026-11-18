#pragma once

#include <cmath>

namespace esphome
{
  namespace number
  {
    class Number
    {
    public:
      virtual ~Number() = default;
      float state{NAN};
      void publish_state(float value) { state = value; }
      void set(float value) { control(value); }

    protected:
      virtual void control(float value) = 0;
    };
  } // namespace number
} // namespace esphome
