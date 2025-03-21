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
            ESP_LOGCONFIG(TAG, "  Requested address: %d", this->requested_sequence_number_);
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
                this->requested_sequence_number_ = 1;
                return;
            }
            
            if (this->requested_sequence_number_ != -1 && this->available() > 0) {
                char response_buffer[20];
                int length = this->read_array(response_buffer, sizeof(response_buffer));
                ESP_LOGD(TAG, "Response: %s", response_buffer);
                this->requested_sequence_number_ = -1;
                this->requested_update_ = false;
            }
            
        }

    }
}