#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome
{
  namespace hlink_ac
  {
    class HlinkAc : public Component, public uart::UARTDevice
    {
    public:
      void setup() override;
      void loop() override;
      void dump_config() override;

    protected:
      unsigned long last_sent_time_ = 0;
      bool requested_update_ = false;
      uint16_t requested_address_ = -1;
      void request_update_();
    };
  }
}