#pragma once

#include "esphome/core/automation.h"
#include "hlink_ac.h"

namespace esphome {
namespace hlink_ac {
template<typename... Ts> class HlinkAcSendHlinkCmd : public Action<Ts...>, public Parented<HlinkAc> {
 public:
  TEMPLATABLE_VALUE(std::string, cmd_type)
  TEMPLATABLE_VALUE(std::string, address)
  TEMPLATABLE_VALUE(optional<std::string>, data)

  void play(Ts... x) override {
    auto cmd_type = this->cmd_type_.value(x...);
    auto address = this->address_.value(x...);
    auto data = this->data_.value(x...);
    this->parent_->send_hlink_cmd(cmd_type, address, data);
  }
};

template<typename... Ts> class ResetAirFilterCleanWarning : public Action<Ts...>, public Parented<HlinkAc> {
 public:
  void play(Ts... x) override { this->parent_->reset_air_filter_clean_warning(); }
};

class SendHlinkCmdResultTrigger : public Trigger<const SendHlinkCmdResult &> {
 public:
  explicit SendHlinkCmdResultTrigger(HlinkAc *parent) {
    parent->add_send_hlink_cmd_result_callback([this](const SendHlinkCmdResult &result) { this->trigger(result); });
  }
};

#ifdef USE_TEXT_SENSOR
template<typename... Ts> class StartDebugDiscovery : public Action<Ts...>, public Parented<HlinkAc> {
 public:
  void play(Ts... x) override { this->parent_->start_debug_discovery(); }
};

template<typename... Ts> class StopDebugDiscovery : public Action<Ts...>, public Parented<HlinkAc> {
 public:
  void play(Ts... x) override { this->parent_->stop_debug_discovery(); }
};
#endif
}  // namespace hlink_ac
}  // namespace esphome