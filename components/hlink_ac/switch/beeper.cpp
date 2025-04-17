#include "beeper.h"

namespace esphome {
namespace hlink_ac {
void BeeperSwitch::write_state(bool state) {
    if (this->state != state) {
        this->publish_state(state);
        this->parent_->handle_beep_state_change(state);
    }
}
}
}