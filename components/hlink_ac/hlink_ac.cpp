#include "esphome/core/log.h"
#include "hlink_ac.h"

namespace esphome
{
    namespace hlink_ac
    {
        static const char *const TAG = "hlink_ac";

        void HlinkAc::setup()
        {
            ESP_LOGD(TAG, "Hlink AC component initialized.");
        }


        void HlinkAc::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Hlink AC component:");
        }

        void HlinkAc::loop()
        {
            unsigned long current_time = millis();
            if (current_time - last_sent_time_ >= 10000)
            {
                send_uart_command();
                sent_counter_++;
                last_sent_time_ = current_time;
                ESP_LOGD(TAG, "Loop %d", sent_counter_);
            }
        }

        void HlinkAc::send_uart_command()
        {
            const uint8_t command[] = {0x00};
            this->write_array(command, sizeof(command));
        }

    }
}