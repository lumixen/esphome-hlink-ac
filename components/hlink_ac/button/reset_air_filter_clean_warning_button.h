#pragma once

#include "esphome/components/button/button.h"
#include "../hlink_ac.h"

namespace esphome {
namespace hlink_ac {

class ResetAirFilterCleanWarningButton : public button::Button, public Parented<HlinkAc> {
 public:
  ResetAirFilterCleanWarningButton() = default;

 protected:
  void press_action() override;
};

}  // namespace hlink_ac
}  // namespace esphome
