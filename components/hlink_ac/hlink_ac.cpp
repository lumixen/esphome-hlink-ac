#include <sstream>
#include <iomanip>
#include "esphome/core/log.h"
#include "hlink_ac.h"

namespace esphome
{
    namespace hlink_ac
    {
        static const char *const TAG = "hlink_ac";
        static const uint8_t CMD_TERMINATION_SYMBOL = 0x0D;
    
        FeatureType features[] = { POWER_STATE, MODE, TARGET_TEMP };
        constexpr int features_size = sizeof(features) / sizeof(features[0]);

        void HlinkAc::setup()
        {
            this->set_interval(6500, [this] { this->request_status_update_(); });
            ESP_LOGD(TAG, "Hlink AC component initialized.");
        }


        void HlinkAc::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Hlink AC component:");
            ESP_LOGCONFIG(TAG, "  Requested sequence: %d", this->requested_feature_);
        }

        void HlinkAc::request_status_update_()
        {
            if (this->requested_feature_ != -1) {
                this->requested_feature_ = 0;
                this->requested_update_ms = millis();
            }
        }

        void HlinkAc::loop()
        {
            if (this->requested_feature_ != -1 && !this->receiving_response_) {
                this->send_status_update_request_(features[this->requested_feature_]);
            }
            
            if (this->requested_feature_ != -1) {
                this->read_status_update_();
            }
            
        }
        
        void HlinkAc::send_status_update_request_(FeatureType feature_type) {
            uint16_t p_value = feature_type;
            uint16_t c_value = p_value ^ 0xFFFF; // Calculate checksum
            char buf[18] = {0};
            int size = sprintf(buf, "MT P=%04X C=%04X\x0D", p_value, c_value);
            this->write_str(buf);
            // ESP_LOGD(TAG, "Requested address %04X", p_value);
        }

        void HlinkAc::read_status_update_() {
            if (this->available() > 2) {
                uint8_t response_buffer[30] = {0};
                int index = 0;
                while (response_buffer[index] != CMD_TERMINATION_SYMBOL && millis() - requested_update_ms < 4000) {
                    this->read_byte(&response_buffer[++index]);
                }
                // Request next feature from features sequence or reset to none if done
                if (this->requested_feature_ + 1 >= features_size) {
                    this->requested_feature_ = -1;
                } else {
                    this->requested_feature_++;
                }
            }
        }
    }
}