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

        const HlinkResponseFrame HLINK_RESPONSE_NOTHING = {HlinkResponseFrame::Status::PROCESSING};
        const HlinkResponseFrame HLINK_RESPONSE_INVALID = {HlinkResponseFrame::Status::INVALID};

        static const std::string OK_TOKEN = "OK";
        static const std::string NG_TOKEN = "NG";

        // static const std::map<uint32_t,

        // AC status features
        FeatureType features[] = {POWER_STATE, MODE, TARGET_TEMP, SWING_MODE, FAN_MODE};
        constexpr int features_size = sizeof(features) / sizeof(features[0]);

        void HlinkAc::setup()
        {
            this->set_interval(STATUS_UPDATE_INTERVAL, [this]
                               { this->request_status_update_(); });
            this->set_timeout(10000, [this]
                              { this->test_st_(); });
            ESP_LOGD(TAG, "Hlink AC component initialized.");
        }

        void HlinkAc::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Hlink AC component:");
        }

        void HlinkAc::test_st_()
        {
            write_hlink_frame_({HlinkRequestFrame::Type::ST,
                                {0x0000,
                                 0x01,
                                 HlinkRequestFrame::AttributeFormat::TWO_DIGITS}});
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
                HlinkResponseFrame response = this->read_cmd_response_(50);
                switch (response.status)
                {
                case HlinkResponseFrame::Status::OK:
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
                case HlinkResponseFrame::Status::INVALID:
                case HlinkResponseFrame::Status::NG:
                    this->status_.state = IDLE;
                }
            }

            // Reset status to IDLE if we reached timeout deadline
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
            write_hlink_frame_({HlinkRequestFrame::Type::MT,
                                {feature_type}});

            // uint16_t p_value = feature_type;
            // uint16_t c_value = p_value ^ 0xFFFF; // Calculate checksum
            // char buf[18] = {0};
            // int size = sprintf(buf, "MT P=%04X C=%04X\x0D", p_value, c_value);
            // this->write_str(buf);
        }

        void HlinkAc::write_hlink_frame_(HlinkRequestFrame frame)
        {
            const char *message_type = frame.type == HlinkRequestFrame::Type::MT ? "MT" : "ST";
            uint8_t message_size = 17;
            if (frame.p.secondary.has_value() && frame.p.secondary_format.value() == HlinkRequestFrame::AttributeFormat::TWO_DIGITS)
            {
                message_size = 20;
            }
            else if (frame.p.secondary.has_value() && frame.p.secondary_format.value() == HlinkRequestFrame::AttributeFormat::FOUR_DIGITS)
            {
                message_size = 22;
            }
            char message_buf[message_size] = {0};
            uint16_t checksum = frame.p.first + frame.p.secondary.value_or(0) ^ 0xFFFF;
            if (message_size == 17)
            {
                sprintf(message_buf, "%s P=%04X C=%04X\x0D", message_type, frame.p.first, checksum);
            }
            else if (message_size == 20)
            {
                sprintf(message_buf, "%s P=%04X,%02X C=%04X\x0D", message_type, frame.p.first, frame.p.secondary_format.value(), checksum);
            }
            else if (message_size == 22)
            {
                sprintf(message_buf, "%s P=%04X,%04X C=%04X\x0D", message_type, frame.p.first, frame.p.secondary_format.value(), checksum);
            }

            // std::string request = frame.type == HlinkRequestFrame::Type::MT ? "MT P=" : "ST P=";
            // char program_first[4];
            // sprintf(program_first, "%04X", frame.p.first);
            // request.append(program_first);
            // if (frame.p.secondary.has_value()) {
            //     if (frame.p.secondary_format.value() == HlinkRequestFrame::SecondaryProgramAttributeFormat::TWO_DIGITS) {
            //         char program_second[2];
            //         sprintf(program_second, ",%02X", frame.p.secondary.value());
            //         request.append(program_second);
            //     } else {
            //         char program_second[4];
            //         sprintf(program_second, ",%04X", frame.p.secondary.value());
            //         request.append(program_second);
            //     }
            //     request.append(program_first);
            // }
            // uint16_t checksum = frame.p.first^ 0xFFFF

            // char program
            // if (frame.p.secondary.has_value())
            // // char buf[30] = {0};

            // request.append(std::to_string(frame.p.first));
            // int size = sprintf(buf, "ST P=%04X,%04X C=%04X\x0D",
            //                    frame.p.first, frame.p.secondary.value_or(0),
            //                    frame.p.first ^ 0xFFFF);
            this->write_str(message_buf);
            // char buf[18] = {0};
            // int size = sprintf(buf, "MT P=%04X C=%04X\x0D", frame.p_value, frame.p_value ^ 0xFFFF);
            // this->write_str(buf);
        }

        HlinkResponseFrame HlinkAc::read_cmd_response_(uint32_t timeout_ms)
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

                HlinkResponseFrame::Status status;
                if (response_tokens[0] == OK_TOKEN)
                {
                    status = HlinkResponseFrame::Status::OK;
                }
                else if (response_tokens[0] == NG_TOKEN)
                {
                    status = HlinkResponseFrame::Status::NG;
                }
                else
                {
                    return HLINK_RESPONSE_INVALID;
                }
                uint32_t p_value = std::stoi(response_tokens[1], nullptr, 16);
                uint16_t checksum = std::stoi(response_tokens[2], nullptr, 16);
                ESP_LOGD(TAG, "Received H-link response. Status: %s, P: %04X, C: %04X", status == HlinkResponseFrame::Status::OK ? "OK" : "NG", p_value, checksum);
                return {status, p_value, checksum};
            }
            return HLINK_RESPONSE_NOTHING;
        }
    }
}