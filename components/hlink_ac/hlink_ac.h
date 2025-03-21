#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome
{
  namespace hlink_ac
  {
    class HlinkAcComponent : public Component, public uart::UARTDevice
    {
    public:
      void setup() override;
      void loop() override;

    private:
      unsigned long last_sent_time_ = 0;
      void send_uart_command();
    };
  }
}