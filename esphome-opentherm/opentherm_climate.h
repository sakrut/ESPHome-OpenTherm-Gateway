#pragma once
#include "esphome.h"
#include <functional>

class OpenthermClimate : public Climate, public Component {
private:
    const char *TAG = "opentherm_climate";
    float default_target_ = 0;
    std::function<void(float)> targetTemperatureSetter_;
public:

    void setup() override {
        this->mode = climate::CLIMATE_MODE_HEAT;
        
        this->target_temperature = default_target_;
    }

    void setTargetTemperatureSetter(const std::function<void(float)>& setter) {
        targetTemperatureSetter_ = setter;
    }


    climate::ClimateTraits traits() override {

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

    void control(const ClimateCall &call) override {

        if (call.get_mode().has_value()) {
            // User requested mode change
            ClimateMode mode = *call.get_mode();
            // Send mode to hardware
            // ...
            ESP_LOGD(TAG, "get_mode");    

            // Publish updated state
            this->mode = mode;
            this->publish_state();
        }
        if (call.get_target_temperature().has_value()) {
            // User requested target temperature change
            float temp = *call.get_target_temperature();
            // Send target temp to climate
            // ...
            ESP_LOGD(TAG, "get_target_temperature");    

            this->target_temperature = temp;
            if (targetTemperatureSetter_) {
                targetTemperatureSetter_(temp);  // Ustawianie temperatury w delegacie
            }
            this->publish_state();
        }
        if (call.get_target_temperature_low().has_value()) {
            // User requested target temperature change
            float temp = *call.get_target_temperature_low();
            // Send target temp to climate
            // ...
            ESP_LOGD(TAG, "get_target_temperature_low");    

            this->target_temperature_low = temp;
            this->publish_state();
        }
        if (call.get_target_temperature_high().has_value()) {
            // User requested target temperature change
            float temp = *call.get_target_temperature_high();
            // Send target temp to climate
            // ...
            ESP_LOGD(TAG, "get_target_temperature_high");    

            this->target_temperature_high = temp;
            this->publish_state();
        }

    }
};
