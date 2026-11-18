#pragma once

#include <cmath>
#include <cstdint>

inline unsigned long fake_millis = 0;
inline unsigned long millis() { return fake_millis; }
inline void delay(unsigned long ms) { fake_millis += ms; }

namespace esphome
{
  class Component
  {
  public:
    virtual ~Component() = default;
    virtual void setup() {}
    virtual void loop() {}
    virtual void dump_config() {}
  };

  class PollingComponent : public Component
  {
  public:
    explicit PollingComponent(uint32_t) {}
    virtual void update() {}
  };
} // namespace esphome
