#include "auto_target_temperature_offset.h"

namespace esphome {
namespace hlink_ac {

void AutoTargetTemperatureOffsetNumber::control(float value) { this->parent_->set_auto_temperature_offset(value); }

}  // namespace hlink_ac
}  // namespace esphome