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

        const HlinkResponse HLINK_RESPONSE_NOTHING = {HlinkResponse::Status::PROCESSING};
        const HlinkResponse HLINK_RESPONSE_INVALID = {HlinkResponse::Status::INVALID};

        static const std::string OK_TOKEN = "OK";
        static const std::string NG_TOKEN = "NG";

        // static const std::map<uint32_t,

        // AC status features
        FeatureType features[] = {POWER_STATE, MODE, TARGET_TEMP, SWING_MODE, FAN_MODE, DEVICE_SN};
        constexpr int features_size = sizeof(features) / sizeof(features[0]);

        void HlinkAc::setup()
        {
            this->set_interval(STATUS_UPDATE_INTERVAL, [this]
                               { this->request_status_update_(); });
            ESP_LOGD(TAG, "Hlink AC component initialized.");
        }

        void HlinkAc::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Hlink AC component:");
        }

        void HlinkAc::request_status_update_()
        {
            if (this->status_.state == IDLE)
            {
                // Launch update sequence
                this->status_.state = REQUEST_NEXT_FEATURE;
                this->status_.requested_feature = 0;
                this->status_.status_changed_at_ms = millis();
            }
        }

        void HlinkAc::loop()
        {
            if (this->status_.state == REQUEST_NEXT_FEATURE)
            {
                this->write_cmd_request_(features[this->status_.requested_feature]);
                this->status_.state = READ_NEXT_FEATURE;
            }

            if (this->status_.state == READ_NEXT_FEATURE)
            {
                HlinkResponse response = this->read_cmd_response_(50);
                switch (response.status)
                {
                case HlinkResponse::Status::OK:
                    if (this->status_.requested_feature + 1 < features_size)
                    {
                        this->status_.state = REQUEST_NEXT_FEATURE;
                        this->status_.requested_feature++;
                    }
                    else
                    {
                        this->status_.state = IDLE;
                    }
                    break;
                case HlinkResponse::Status::NG:
                    this->status_.state = IDLE;
                }

                // if (response.status == HlinkResponse::Status::OK) {
                //     if (this->status_.requested_feature + 1 < features_size) {
                //         this->status_.state = REQUEST_NEXT_FEATURE;
                //         this->status_.requested_feature++;
                //     } else {
                //         this->status_.state = IDLE;
                //     }
                // } else if (response.status == HlinkResponse::Status::NG) {
                //     this->status_.state = IDLE;
                // }
            }

            // Reset update status if we reached timeout
            if (this->status_.state != IDLE && millis() - this->status_.status_changed_at_ms > STATUS_UPDATE_TIMEOUT)
            {
                this->status_.state = IDLE;
                ESP_LOGW(TAG, "Reached timeout while updating H-link AC status.");
            }
        }

        void HlinkAc::write_cmd_request_(FeatureType feature_type)
        {
            while (this->available())
            {
                // Reset uart buffer before requesting next cmd
                this->read();
            }
            uint16_t p_value = feature_type;
            uint16_t c_value = p_value ^ 0xFFFF; // Calculate checksum
            char buf[18] = {0};
            int size = sprintf(buf, "MT P=%04X C=%04X\x0D", p_value, c_value);
            this->write_str(buf);
        }

        HlinkResponse HlinkAc::read_cmd_response_(uint32_t timeout_ms)
        {
            if (this->available() > 2)
            {
                uint32_t started_millis = millis();
                std::string response(30, '\0');
                int read_index = 0;
                // Read response unless carriage return symbol, timeout or buffer overflow
                while (millis() - started_millis < timeout_ms || read_index < 30)
                {
                    this->read_byte((uint8_t *)&response[read_index]);
                    if (response[read_index] == CMD_TERMINATION_SYMBOL)
                    {
                        break;
                    }
                    read_index++;
                }
                std::vector<std::string> response_tokens;
                for (int i = 0, last_space_i = 0; i <= read_index; i++)
                {
                    if (response[i] == ' ' || response[i] == '\r')
                    {
                        uint8_t pos_shift = last_space_i > 0 ? 2 : 0; // Shift ahead to remove 'X=' from the tokens after initial OK/NG
                        response_tokens.push_back(response.substr(last_space_i + pos_shift, i - last_space_i - pos_shift));
                        last_space_i = i + 1;
                    }
                }
                // ESP_LOGD(TAG, "Response tokens size: %d", response_tokens.size());
                // for (int i = 0; i < response_tokens.size(); i++) {
                //     ESP_LOGD(TAG, "Token %d: %s", i, response_tokens[i].c_str());
                // }
                if (response_tokens.size() != 3)
                {
                    ESP_LOGW(TAG, "Invalid H-link response: %s", response.c_str());
                    return HLINK_RESPONSE_INVALID;
                }

                HlinkResponse::Status status;
                if (response_tokens[0] == OK_TOKEN)
                {
                    status = HlinkResponse::Status::OK;
                }
                else if (response_tokens[0] == NG_TOKEN)
                {
                    status = HlinkResponse::Status::NG;
                }
                else
                {
                    return HLINK_RESPONSE_INVALID;
                }
                uint32_t p_value = std::stoi(response_tokens[1], nullptr, 16);
                uint16_t checksum = std::stoi(response_tokens[2], nullptr, 16);
                ESP_LOGD(TAG, "Received H-link response. Status: %s, P: %04X, C: %04X", status == HlinkResponse::Status::OK ? "OK" : "NG", p_value, checksum);
                return {status, p_value, checksum};
            }
            return HLINK_RESPONSE_NOTHING;
        }
    }
}