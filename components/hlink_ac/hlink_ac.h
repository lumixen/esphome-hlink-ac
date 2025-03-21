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
      unsigned long sent_counter_ = 0;
      void send_uart_command();
    };
  }
}