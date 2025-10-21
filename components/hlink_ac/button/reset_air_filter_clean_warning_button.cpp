#include "reset_air_filter_clean_warning_button.h"

namespace esphome {
namespace hlink_ac {

void ResetAirFilterCleanWarningButton::press_action() { this->parent_->reset_air_filter_clean_warning(); }

}  // namespace hlink_ac
}  // namespace esphome
