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
            this->set_interval(6500, [this] { this->request_status_update_(); });
            ESP_LOGD(TAG, "Hlink AC component initialized.");
        }


        void HlinkAc::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Hlink AC component:");
            ESP_LOGCONFIG(TAG, "  Requested update: %s", this->requested_update_ ? "true" : "false");
            ESP_LOGCONFIG(TAG, "  Requested sequence: %d", this->requested_sequence_number_);
        }

        void HlinkAc::request_status_update_()
        {
            this->requested_update_ = true;
        }

        void HlinkAc::loop()
        {
            if (this->requested_update_ && this->requested_sequence_number_ == -1) {
                uint16_t p_value = 1;
                uint16_t c_value = p_value ^ 0xFFFF;
                char buf[20] = {0};
                char test[] = "ASDASSDASD";
                int size = sprintf(buf, "MT P=%04X C=%04X\x0D", p_value, c_value);
                this->write_str(buf);
                ESP_LOGD(TAG, "Wrote: %s", test);
                this->requested_sequence_number_ = 1;
                return;
            }
            
            if (this->requested_sequence_number_ != -1 && this->available() > 0) {
                const int MAX_BUFFER_SIZE = 30;  // Define max buffer size
                uint8_t response_buffer[MAX_BUFFER_SIZE] = {0};  // Fixed-size buffer
                int index = 0;
            
                while (this->available() && index < MAX_BUFFER_SIZE - 1) {  
                    this->read_byte(&response_buffer[index]);
                    if (response_buffer[index] == 0x0D) {  // Stop reading when stop byte (0x0D) is encountered
                        break;
                    }
                    index++;
                }

                response_buffer[++index] = '\0';  // Null-terminate for logging
                ESP_LOGD(TAG, "Read %d bytes", index);
                // ESP_LOGD(TAG, "Response: %s", response_buffer);
                this->requested_sequence_number_ = -1;
                this->requested_update_ = false;
            }
            
        }

    }
}