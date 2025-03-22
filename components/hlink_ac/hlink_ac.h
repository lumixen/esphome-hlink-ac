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

    enum State : uint8_t {
      PENDING,
      REQUEST_NEXT_FEATURE,
      READ_NEXT_FEATURE,
    };

    struct HlinkAcStatus {
      State state = PENDING;
      uint32_t status_changed_at_ms = 0;
      int8_t requested_feature = 0;
    };
    

    class HlinkAc : public Component, public uart::UARTDevice
    {
    public:
      void setup() override;
      void loop() override;
      void dump_config() override;

    protected:
      HlinkAcStatus status_ = HlinkAcStatus();
      void request_status_update_();
      void write_cmd_request_(FeatureType feature_type);
      bool read_cmd_response_(uint32_t timeout_ms);
    };
  }
}