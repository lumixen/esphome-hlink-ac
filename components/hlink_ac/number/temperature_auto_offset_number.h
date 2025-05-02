#pragma once

#include "esphome/components/number/number.h"
#include "../hlink_ac.h"

namespace esphome {
namespace hlink_ac {
class TemperatureAutoOffsetNumber : public number::Number, public Parented<HlinkAc> {
 public:
  TemperatureAutoOffsetNumber() = default;

 protected:
  void control(float value) override;
};
}  // namespace hlink_ac
}  // namespace esphome