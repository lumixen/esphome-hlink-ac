#include "beeper.h"

namespace esphome {
namespace hlink_ac {
void BeeperSwitch::write_state(bool state) {
    if (this->state != state) {
        this->parent_->enqueue_beeper_state_action(state);
    }
}
}
}