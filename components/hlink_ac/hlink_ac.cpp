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
        static const uint32_t STATUS_UPDATE_INTERVAL = 6500;
        static const uint32_t STATUS_UPDATE_TIMEOUT = 2000;
    
        // Status update AC features
        FeatureType features[] = { POWER_STATE, MODE, TARGET_TEMP, SWING_MODE, FAN_MODE };
        constexpr int features_size = sizeof(features) / sizeof(features[0]);

        void HlinkAc::setup()
        {
            this->set_interval(STATUS_UPDATE_INTERVAL, [this] { this->request_status_update_(); });
            ESP_LOGD(TAG, "Hlink AC component initialized.");
        }


        void HlinkAc::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Hlink AC component:");
        }

        void HlinkAc::request_status_update_()
        {
            if (this->status_.state == PENDING) {
                // Begin update sequence
                this->status_.state = REQUEST_NEXT_FEATURE;
                this->status_.requested_feature = 0;
                this->status_.status_changed_at_ms = millis();
            }
        }

        void HlinkAc::loop()
        {
            if (this->status_.state = REQUEST_NEXT_FEATURE) {
                this->write_cmd_request_(features[this->status_.requested_feature]);
                this->status_.state = READ_NEXT_FEATURE;
            }
            
            if (this->status_.state == READ_NEXT_FEATURE) {
                bool success = this->read_status_(50);
                if (success) {
                    if (this->status_.requested_feature + 1 < features_size) {
                        this->status_.state = REQUEST_NEXT_FEATURE;
                        this->status_.requested_feature++;
                    } else {
                        this->status_.state = PENDING;
                    }
                }
            }
            
            if (this->status_.state != PENDING && millis() - this->status_.status_changed_at_ms > STATUS_UPDATE_TIMEOUT) {
                this->status_.state = PENDING;
            }
        }
        
        void HlinkAc::write_cmd_request_(FeatureType feature_type) {
            while (this->available()) {
                // Reset uart buffer before requesting next cmd
                this->read();
            }
            uint16_t p_value = feature_type;
            uint16_t c_value = p_value ^ 0xFFFF; // Calculate checksum
            char buf[18] = {0};
            int size = sprintf(buf, "MT P=%04X C=%04X\x0D", p_value, c_value);
            this->write_str(buf);
        }

        bool HlinkAc::read_status_(uint32_t timeout_ms) {
            if (this->available() > 2) {
                uint32_t started_millis = millis();
                uint8_t response_buffer[30] = {0};
                int index = 0;
                // Read response unless termination symbol or timeout
                while (response_buffer[index] != CMD_TERMINATION_SYMBOL) {
                    if (millis() - started_millis > timeout_ms) {
                        return false;
                    }
                    this->read_byte(&response_buffer[++index]);
                }
                return true;
            }
            return false;
        }
    }
}