#include "opentherm_button.h"
#include "esphome/core/log.h"

namespace esphome
{
  namespace opentherm
  {

    static const char *const TAG = "opentherm.button";

    void OpenthermResetButton::press_action()
    {
      ESP_LOGI(TAG, "Reset button pressed");

      if (parent_ != nullptr)
      {
        bool success = parent_->sendBoilerReset();
        if (success)
        {
          ESP_LOGI(TAG, "Boiler reset command sent successfully");
        }
        else
        {
          ESP_LOGW(TAG, "Boiler reset command failed");
        }
      }
      else
      {
        ESP_LOGE(TAG, "No parent component set for reset button");
      }
    }

  } // namespace opentherm
} // namespace esphome
