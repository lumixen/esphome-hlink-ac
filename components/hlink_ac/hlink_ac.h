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
      // void set_uart_parent(uart::UARTComponent *parent) { this->uart_channel_ = parent; }

    private:
      unsigned long last_sent_time_ = 0;
      // uart::UARTComponent *uart_;
      void send_uart_command();
    };
  }
}