#pragma once

#include <gtest/gtest.h>

#include "esphome/components/hlink_ac/hlink_ac.h"

namespace esphome::hlink_ac::testing {

class TestHlinkAc : public HlinkAc {
 public:
  using HlinkAc::is_auto_temperature_mode_;
  using HlinkAc::clamp_auto_temperature_;
  using HlinkAc::encode_auto_temperature_;
  using HlinkAc::is_nanable_equal_;
  using HlinkAc::format_target_temperature_log_;

  void set_reference_temperature(float ref) { this->reference_temperature_ = ref; }
};

}  // namespace esphome::hlink_ac::testing