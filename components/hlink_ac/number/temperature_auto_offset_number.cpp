#include "temperature_auto_offset_number.h"

namespace esphome {
namespace hlink_ac {
void TemperatureAutoOffsetNumber::control(float value) {
  this->parent_->set_auto_temperature_offset(value);
}
}  // namespace hlink_ac
}  // namespace esphome