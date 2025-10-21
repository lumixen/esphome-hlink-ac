#pragma once

#include "esphome/core/automation.h"
#include "hlink_ac.h"

namespace esphome {
namespace hlink_ac {
template<typename... Ts> class HlinkAcSendHlinkCmd : public Action<Ts...>, public Parented<HlinkAc> {
 public:
  TEMPLATABLE_VALUE(std::string, address)
  TEMPLATABLE_VALUE(std::string, data)

  void play(Ts... x) override {
    auto address = this->address_.value(x...);
    auto data = this->data_.value(x...);
    this->parent_->send_hlink_cmd(address, data);
  }
};

class SendHlinkCmdResultTrigger : public Trigger<std::string> {
 public:
  explicit SendHlinkCmdResultTrigger(HlinkAc *parent) {
    parent->add_send_hlink_cmd_result_callback([this](std::string data) { this->trigger(data); });
  }
};
}  // namespace hlink_ac
}  // namespace esphome