#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/preferences.h"
#include "../hlink_ac.h"

namespace esphome {
namespace hlink_ac {
class AutoTargetTemperatureOffsetNumber : public number::Number, public Parented<HlinkAc> {
 public:
  AutoTargetTemperatureOffsetNumber() = default;

 protected:
  void control(float value) override;
};
}  // namespace hlink_ac
}  // namespace esphome