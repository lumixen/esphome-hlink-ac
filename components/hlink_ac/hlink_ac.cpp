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
                char buffer[20];
                sprintf(buffer, "MT P=%04X C=%04X\x0D\x00", p_value, c_value);
                this->write_str(buffer);
                ESP_LOGD(TAG, "Wrote: %s", 20, buffer);
                this->requested_sequence_number_ = 1;
                return;
            }
            
            if (this->requested_sequence_number_ != -1 && this->available() > 0) {
                const int MAX_BUFFER_SIZE = 30;  // Define max buffer size
                uint8_t response_buffer[MAX_BUFFER_SIZE] = {0};  // Fixed-size buffer
                int index = 0;
            
                while (this->available() > 0 && index < MAX_BUFFER_SIZE - 1) {  
                    uint8_t byte = this->read();  // Read one byte
                    response_buffer[index++] = byte;
            
                    if (byte == 0x0D) {  // Stop reading when stop byte (0x0D) is encountered
                        break;
                    }
                }

                response_buffer[index] = '\0';  // Null-terminate for logging
                ESP_LOGD(TAG, "Response: %s", MAX_BUFFER_SIZE, response_buffer);
                this->requested_sequence_number_ = -1;
                this->requested_update_ = false;
            }
            
        }

    }
}