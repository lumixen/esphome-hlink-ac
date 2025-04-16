#pragma once

#include "esphome/components/switch/switch.h"
#include "../hlink_ac.h"

namespace esphome {
namespace hlink_ac {
class RemoteLockSwitch : public switch_::Switch, public Parented<HlinkAc> {
 public:
 RemoteLockSwitch() = default;

 protected:
   void write_state(bool state) override;
};
}
}