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
            if (this->requested_address_ != -1) {
                uint16_t p_value = 1;
                uint16_t c_value = p_value ^ 0xFFFF;
                char buffer[20];
                sprintf(buffer, "MT P=%04X C=%04X\x0D\x00", p_value, c_value);
                this->write_str(buffer);
                this->requested_address_ = 1;
            }
            
            if (this->requested_address_ != -1 && this->available() > 0) {
                uint8_t response_buffer[20];
                int length = this->read_array(response_buffer, sizeof(response_buffer));
                ESP_LOGD(TAG, "Response: %s", response_buffer);
                this->requested_address_ = -1;
            }
            
        }

    }
}