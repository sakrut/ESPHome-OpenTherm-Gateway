#include "opentherm_climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace opentherm {

static const char *const TAG = "opentherm.climate";

void OpenthermClimate::setup() {
  this->mode = climate::CLIMATE_MODE_HEAT;
  this->target_temperature = default_target_temperature_;
}

void OpenthermClimate::dump_config() {
  LOG_CLIMATE("", "OpenTherm Climate", this);
}

climate::ClimateTraits OpenthermClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(true);
  traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT});
  traits.set_supports_two_point_target_temperature(false);
  traits.set_supports_action(true);
  
  traits.set_visual_min_temperature(5);
  traits.set_visual_max_temperature(80);
  traits.set_visual_temperature_step(1);
  
  return traits;
}

void OpenthermClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    esphome::climate::ClimateMode mode = *call.get_mode(); 
    ESP_LOGD(TAG, "Setting mode to %d", mode);
    this->mode = mode;
    this->publish_state();
  }
  
  if (call.get_target_temperature().has_value()) {
    float temp = *call.get_target_temperature();
    ESP_LOGD(TAG, "Setting target temperature to %.1f", temp);
    this->target_temperature = temp;
    
    if (target_temperature_setter_) {
      target_temperature_setter_(temp);
    }
    
    this->publish_state();
  }
}

}  // namespace opentherm
}  // namespace esphome
