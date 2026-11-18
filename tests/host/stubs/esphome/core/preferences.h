#pragma once

#include <cstdint>
#include <cstring>
#include <map>
#include <vector>

namespace esphome
{
  inline std::map<uint32_t, std::vector<uint8_t>> fake_preferences;

  class ESPPreferenceObject
  {
  public:
    ESPPreferenceObject() = default;
    explicit ESPPreferenceObject(uint32_t key) : key_(key), valid_(true) {}

    template <typename T>
    bool save(const T *src)
    {
      if (!valid_)
        return false;
      auto bytes = reinterpret_cast<const uint8_t *>(src);
      fake_preferences[key_].assign(bytes, bytes + sizeof(T));
      return true;
    }

    template <typename T>
    bool load(T *dest)
    {
      auto it = fake_preferences.find(key_);
      if (!valid_ || it == fake_preferences.end() || it->second.size() != sizeof(T))
        return false;
      std::memcpy(dest, it->second.data(), sizeof(T));
      return true;
    }

  private:
    uint32_t key_{0};
    bool valid_{false};
  };

  struct ESPPreferences
  {
    template <typename T>
    ESPPreferenceObject make_preference(uint32_t key) { return ESPPreferenceObject(key); }
  };
  inline ESPPreferences fake_global_preferences;
  inline ESPPreferences *global_preferences = &fake_global_preferences;
} // namespace esphome
