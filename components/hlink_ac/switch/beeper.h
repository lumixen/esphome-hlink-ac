#pragma once

#include "esphome/components/switch/switch.h"
#include "../hlink_ac.h"

namespace esphome {
namespace hlink_ac {
class BeeperSwitch : public switch_::Switch, public Parented<HlinkAc> {
 public:
 BeeperSwitch() = default;

 protected:
   void write_state(bool state) override;
};
}
}