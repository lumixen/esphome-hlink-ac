#include <sstream>
#include <iomanip>
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
            int a_value = 1;
            int c_value = a_value ^ 0xFF;
            std::ostringstream oss;
            oss << std::uppercase << "MT P=" << std::setw(4) << std::setfill('0') << std::hex << a_value 
                << " C=" << std::setw(4) << std::setfill('0') << std::hex << c_value << (char)0x0D;
            this->write_str(oss.str().c_str());
        }

    }
}