#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "opentherm_component.h"

namespace esphome
{
  namespace opentherm
  {

    class OpenthermResetButton : public button::Button, public Component
    {
    public:
      void set_parent(OpenthermComponent *parent) { parent_ = parent; }

    protected:
      void press_action() override;
      OpenthermComponent *parent_{nullptr};
    };

  } // namespace opentherm
} // namespace esphome
