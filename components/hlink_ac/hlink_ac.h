#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome
{
  namespace hlink_ac
  {
    enum FeatureType : uint16_t {
      POWER_STATE = 0x0000,
      MODE = 0x0001,
      TARGET_TEMP = 0x0003,
      SWING_MODE = 0x0014,
      FAN_MODE = 0x0002
    };

    class HlinkAc : public Component, public uart::UARTDevice
    {
    public:
      void setup() override;
      void loop() override;
      void dump_config() override;

    protected:
      bool receiving_response_ = false;
      int8_t requested_feature_ = -1;
      uint32_t started_status_update_ms_ = 0;
      void request_status_update_();
      void write_cmd_request_(FeatureType feature_type);
      void read_status_(uint16_t timeout_ms);
    };
  }
}