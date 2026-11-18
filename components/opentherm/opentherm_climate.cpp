#include "opentherm_climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace opentherm {

static const char *const TAG = "opentherm.climate";

void OpenthermClimate::setup() {
  // Own key, not restore_state_(): firmware before CH blocking saved Off as a no-op, restoring it would block CH
  mode_pref_ = global_preferences->make_preference<uint8_t>(0x0C4B0000u + static_cast<uint32_t>(climate_type_));
  uint8_t saved;
  this->mode = mode_pref_.load(&saved) && saved == climate::CLIMATE_MODE_OFF ? climate::CLIMATE_MODE_OFF
                                                                              : climate::CLIMATE_MODE_HEAT;
  // Don't set target_temperature to 0 - leave it as NAN until initialized
  // from boiler or set by user
  if (default_target_temperature_ > 0) {
    this->target_temperature = default_target_temperature_;
    target_temperature_initialized_ = true;
  }
}

void OpenthermClimate::dump_config() {
  LOG_CLIMATE("", "OpenTherm Climate", this);
}

climate::ClimateTraits OpenthermClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT});
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_ACTION);

  if (read_only_) {
    traits.set_visual_min_temperature(5);
    traits.set_visual_max_temperature(35);
    traits.set_visual_temperature_step(0.5);
  } else {
    traits.set_visual_min_temperature(5);
    traits.set_visual_max_temperature(80);
    traits.set_visual_temperature_step(1);
  }
  
  return traits;
}

void OpenthermClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    esphome::climate::ClimateMode mode = *call.get_mode(); 
    ESP_LOGD(TAG, "Setting mode to %d", mode);
    this->mode = mode;
    uint8_t saved = mode;
    mode_pref_.save(&saved);
    this->publish_state();
  }
  
  if (call.get_target_temperature().has_value() && !read_only_) {
    float temp = *call.get_target_temperature();
    ESP_LOGD(TAG, "Setting target temperature to %.1f", temp);
    this->target_temperature = temp;
    target_temperature_initialized_ = true;  // Mark as user-set

    if (target_temperature_setter_) {
      target_temperature_setter_(temp);
    }

    this->publish_state();
  }
}

}  // namespace opentherm
}  // namespace esphome
