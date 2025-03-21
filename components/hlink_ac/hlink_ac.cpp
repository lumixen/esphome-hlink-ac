#include "esphome/core/log.h"
#include "hlink_ac.h"

namespace esphome
{
    namespace hlink_ac
    {

        void HlinkAcComponent::setup()
        {
            ESP_LOGD("hlink_ac", "Hlink AC component initialized.");
        }

        void HlinkAcComponent::loop()
        {
            unsigned long current_time = millis();
            if (current_time - last_sent_time_ >= 10000)
            { // 10-second interval
                send_uart_command();
                last_sent_time_ = current_time;
            }
        }

        void HlinkAcComponent::send_uart_command()
        {
            const uint8_t command[] = {0x00};
            this->write_array(command, sizeof(command));
            ESP_LOGD("hlink_ac", "Sent UART command: 0x00");
        }

    }
}