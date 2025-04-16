#include "remote_lock.h"

namespace esphome {
namespace hlink_ac {
void RemoteLockSwitch::write_state(bool state) {
    if (this->state != state) {
        this->parent_->enqueue_remote_lock_action(state);
    }
}
}
}