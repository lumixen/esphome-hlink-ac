#include "esphome/core/log.h"
#include "hlink_ac.h"

namespace esphome
{
    namespace hlink_ac
    {
        static const char *const TAG = "hlink_ac";

        static const uint8_t CMD_TERMINATION_SYMBOL = 0x0D;
        static const uint32_t STATUS_UPDATE_INTERVAL = 5000;
        static const uint32_t STATUS_UPDATE_TIMEOUT = 2000;

        static const uint8_t HLINK_MSG_READ_BUFFER_SIZE = 35;

        const HlinkResponseFrame HLINK_RESPONSE_PROCESSING = {HlinkResponseFrame::Status::PROCESSING};
        const HlinkResponseFrame HLINK_RESPONSE_INVALID = {HlinkResponseFrame::Status::INVALID};

        static const std::string OK_TOKEN = "OK";
        static const std::string NG_TOKEN = "NG";

        // AC status features
        FeatureType features[] = {POWER_STATE, MODE, TARGET_TEMP, SWING_MODE, FAN_MODE, CURRENT_TEMP};
        constexpr int features_size = sizeof(features) / sizeof(features[0]);

        void HlinkAc::setup()
        {
            this->set_interval(STATUS_UPDATE_INTERVAL, [this]
                               { this->request_status_update_(); });
            ESP_LOGI(TAG, "Hlink AC component initialized.");
        }

        void HlinkAc::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Hlink AC:");
            ESP_LOGCONFIG(TAG, "  Power state: %s", this->hvac_status_.power_state.has_value() ? this->hvac_status_.power_state.value() ? "ON" : "OFF" : "N/A");
            ESP_LOGCONFIG(TAG, "  Mode: %s", this->hvac_status_.mode.has_value() ? LOG_STR_ARG(climate_mode_to_string(this->hvac_status_.mode.value())) : "N/A");
            ESP_LOGCONFIG(TAG, "  Fan mode: %s", this->hvac_status_.fan_mode.has_value() ? LOG_STR_ARG(climate_fan_mode_to_string(this->hvac_status_.fan_mode.value())) : "N/A");
            ESP_LOGCONFIG(TAG, "  Swing mode: %s", this->hvac_status_.swing_mode.has_value() ? LOG_STR_ARG(climate_swing_mode_to_string(this->hvac_status_.swing_mode.value())) : "N/A");
            ESP_LOGCONFIG(TAG, "  Current temperature: %s", this->hvac_status_.current_temperature.has_value() ? std::to_string(this->hvac_status_.current_temperature.value()).c_str() : "N/A");
            ESP_LOGCONFIG(TAG, "  Target temperature: %s", this->hvac_status_.target_temperature.has_value() ? std::to_string(this->hvac_status_.target_temperature.value()).c_str() : "N/A");
        }

        void HlinkAc::request_status_update_()
        {
            if (this->status_.state == IDLE)
            {
                // Launch update sequence
                this->status_.state = REQUEST_NEXT_FEATURE;
                this->status_.requested_feature = 0;
                this->status_.refresh_non_idle_timeout(2000);
            }
        }

        // Returns true if applied request
        // bool HlinkAc::apply_requests_()
        // {
        //     std::unique_ptr<HlinkRequestFrame> request_msg = this->pending_action_requests.dequeue();
        //     if (request_msg != nullptr)
        //     {
        //         this->write_hlink_frame_(*request_msg);
        //         return true;
        //     }
        //     return false;
        // }

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
                    capture_feature_response_to_hvac_status_(
                        features[this->status_.requested_feature],
                        response);
                    if (this->status_.requested_feature + 1 < features_size)
                    {
                        this->status_.state = REQUEST_NEXT_FEATURE;
                        this->status_.requested_feature++;
                    }
                    else
                    {
                        this->status_.state = PUBLISH_CLIMATE_UPDATE_IF_ANY;
                    }
                    break;
                case HlinkResponseFrame::Status::INVALID:
                case HlinkResponseFrame::Status::NG:
                    this->status_.state = IDLE;
                }
                return;
            }

            if (this->status_.state == PUBLISH_CLIMATE_UPDATE_IF_ANY)
            {
                this->publish_climate_update_if_any_();
                this->status_.state = IDLE;
                return;
            }

            if (this->status_.state == APPLY_REQUEST && this->status_.requests_left_to_apply > 0)
            {
                std::unique_ptr<HlinkRequestFrame> request_msg = this->pending_action_requests.dequeue();
                if (request_msg != nullptr)
                {
                    this->write_hlink_frame_(*request_msg);
                    this->status_.requests_left_to_apply--;
                    this->status_.state = ACK_APPLIED_REQUEST;
                } else {
                    this->status_.state = IDLE;
                }
            }
            else
            {
                this->status_.state = IDLE;
            }

            if (this->status_.state == ACK_APPLIED_REQUEST)
            {
                HlinkResponseFrame response = this->read_cmd_response_(50);
                switch (response.status)
                {
                case HlinkResponseFrame::Status::INVALID:
                case HlinkResponseFrame::Status::NG:
                    ESP_LOGW(TAG, "Failed apply action request");
                case HlinkResponseFrame::Status::OK:
                    if (this->status_.requests_left_to_apply > 0)
                    {
                        this->status_.state = APPLY_REQUEST;
                    }
                    else
                    {
                        this->status_.state = IDLE;
                    }
                    break;
                default:
                    break;
                }
                // Update status right away after the applied batch
                if (this->status_.state == IDLE)
                {
                    this->request_status_update_();
                }
            }

            // Reset status to IDLE if we reached timeout deadline
            if (this->status_.state != IDLE && this->status_.reached_timeout_thereshold())
            {
                ESP_LOGW(TAG, "Reached timeout while performing [%d] state action. Reset state to IDLE.", this->status_.state);
                this->status_.reset_state();
            }

            // If there any pending requests - apply them ASAP
            if (this->status_.state == IDLE && this->pending_action_requests.size() > 0)
            {
                this->status_.requests_left_to_apply = this->pending_action_requests.size();
                this->status_.state = APPLY_REQUEST;
                this->status_.refresh_non_idle_timeout(2000);
            }
        }

        void HlinkAc::capture_feature_response_to_hvac_status_(
            FeatureType requested_feature,
            HlinkResponseFrame response)
        {
            switch (requested_feature)
            {
            case FeatureType::POWER_STATE:
                this->hvac_status_.power_state = response.p_value;
                break;
            case FeatureType::MODE:
                if (!this->hvac_status_.power_state.has_value())
                {
                    ESP_LOGW(TAG, "Can't handle climate mode response without power state data");
                    break;
                }
                if (!this->hvac_status_.power_state.value())
                {
                    // Climate mode should be off when device is turned off
                    this->hvac_status_.mode = esphome::climate::ClimateMode::CLIMATE_MODE_OFF;
                    break;
                }
                if (response.p_value == 0x0010 || response.p_value == 0x8010)
                {
                    this->hvac_status_.mode = esphome::climate::ClimateMode::CLIMATE_MODE_HEAT;
                }
                else if (response.p_value == 0x0040 || response.p_value == 0x8040)
                {
                    this->hvac_status_.mode = esphome::climate::ClimateMode::CLIMATE_MODE_COOL;
                }
                else if (response.p_value == 0x0020)
                {
                    this->hvac_status_.mode = esphome::climate::ClimateMode::CLIMATE_MODE_DRY;
                }
                break;
            case FeatureType::TARGET_TEMP:
                this->hvac_status_.target_temperature = response.p_value;
                break;
            case FeatureType::CURRENT_TEMP:
                this->hvac_status_.current_temperature = response.p_value;
                break;
            case FeatureType::SWING_MODE:
                if (response.p_value == 0x0000)
                {
                    this->hvac_status_.swing_mode = esphome::climate::ClimateSwingMode::CLIMATE_SWING_OFF;
                }
                else if (response.p_value == 0x0001)
                {
                    this->hvac_status_.swing_mode = esphome::climate::ClimateSwingMode::CLIMATE_SWING_VERTICAL;
                }
                break;
            case FeatureType::FAN_MODE:
                if (response.p_value == 0x0000)
                {
                    this->hvac_status_.fan_mode = esphome::climate::ClimateFanMode::CLIMATE_FAN_AUTO;
                }
                else if (response.p_value == 0x0001)
                {
                    this->hvac_status_.fan_mode = esphome::climate::ClimateFanMode::CLIMATE_FAN_HIGH;
                }
                else if (response.p_value == 0x0002)
                {
                    this->hvac_status_.fan_mode = esphome::climate::ClimateFanMode::CLIMATE_FAN_MEDIUM;
                }
                else if (response.p_value == 0x0003)
                {
                    this->hvac_status_.fan_mode = esphome::climate::ClimateFanMode::CLIMATE_FAN_LOW;
                }
                else if (response.p_value == 0x0004)
                {
                    this->hvac_status_.fan_mode = esphome::climate::ClimateFanMode::CLIMATE_FAN_QUIET;
                }
                break;
            default:
                break;
            }
        }

        void HlinkAc::publish_climate_update_if_any_()
        {
            if (this->hvac_status_.ready())
            {
                bool should_publish = false;
                if (this->target_temperature != this->hvac_status_.target_temperature.value())
                {
                    this->target_temperature = this->hvac_status_.target_temperature.value();
                    should_publish = true;
                }
                if (this->current_temperature != this->hvac_status_.current_temperature.value())
                {
                    this->current_temperature = this->hvac_status_.current_temperature.value();
                    should_publish = true;
                }
                if (this->mode != this->hvac_status_.mode)
                {
                    this->mode = this->hvac_status_.mode.value();
                    should_publish = true;
                }
                if (this->fan_mode != this->hvac_status_.fan_mode.value())
                {
                    this->fan_mode = this->hvac_status_.fan_mode.value();
                    should_publish = true;
                }
                if (this->swing_mode != this->hvac_status_.swing_mode.value())
                {
                    this->swing_mode = this->hvac_status_.swing_mode.value();
                    should_publish = true;
                }
                if (should_publish)
                {
                    this->publish_state();
                }
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
            std::string message(message_size, 0x00);
            uint16_t checksum = ((frame.p.first >> 8) + (frame.p.first & 0xFF) + (frame.p.secondary.value_or(0) >> 8) + (frame.p.secondary.value_or(0) & 0xFF)) ^ 0xFFFF;
            if (message_size == 17)
            {
                sprintf(&message[0], "%s P=%04X C=%04X\x0D", message_type, frame.p.first, checksum);
            }
            else if (message_size == 20)
            {
                sprintf(&message[0], "%s P=%04X,%02X C=%04X\x0D", message_type, frame.p.first, frame.p.secondary.value(), checksum);
            }
            else if (message_size == 22)
            {
                sprintf(&message[0], "%s P=%04X,%04X C=%04X\x0D", message_type, frame.p.first, frame.p.secondary.value(), checksum);
            }
            this->write_str(message.c_str());
        }

        // Returns PROCESSING state if nothing available on UART input
        HlinkResponseFrame HlinkAc::read_cmd_response_(uint32_t timeout_ms)
        {
            if (this->available())
            {
                uint32_t started_millis = millis();
                std::string response_buf(HLINK_MSG_READ_BUFFER_SIZE, '\0');
                int read_index = 0;
                // Read response unless carriage return symbol, timeout or reasonable buffer size
                while (millis() - started_millis < timeout_ms || read_index < HLINK_MSG_READ_BUFFER_SIZE)
                {
                    this->read_byte((uint8_t *)&response_buf[read_index]);
                    if (response_buf[read_index] == CMD_TERMINATION_SYMBOL)
                    {
                        break;
                    }
                    read_index++;
                }
                std::vector<std::string> response_tokens;
                for (int i = 0, last_space_i = 0; i <= read_index; i++)
                {
                    if (response_buf[i] == ' ' || response_buf[i] == '\r')
                    {
                        uint8_t pos_shift = last_space_i > 0 ? 2 : 0; // Shift ahead to remove 'X=' from the tokens after initial OK/NG
                        response_tokens.push_back(response_buf.substr(last_space_i + pos_shift, i - last_space_i - pos_shift));
                        last_space_i = i + 1;
                    }
                }
                // ESP_LOGD(TAG, "Response tokens size: %d", response_tokens.size());
                // for (int i = 0; i < response_tokens.size(); i++) {
                //     ESP_LOGD(TAG, "Token %d: %s", i, response_tokens[i].c_str());
                // }
                if (response_tokens.size() != 3)
                {
                    ESP_LOGW(TAG, "Invalid H-link response: %s", response_buf.c_str());
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
                if (response_tokens[1].size() > 8)
                {
                    ESP_LOGW(TAG, "Couldn't parse P= value, it's too large: %s", response_tokens[1].c_str());
                    return HLINK_RESPONSE_INVALID;
                }
                uint32_t p_value = std::stoi(response_tokens[1], nullptr, 16);
                uint16_t checksum = std::stoi(response_tokens[2], nullptr, 16);
                return {status, p_value, checksum};
            }
            return HLINK_RESPONSE_PROCESSING;
        }

        void HlinkAc::control(const esphome::climate::ClimateCall &call)
        {
            if (call.get_mode().has_value())
            {
                climate::ClimateMode mode = *call.get_mode();
                switch (mode)
                {
                case climate::ClimateMode::CLIMATE_MODE_OFF:
                    this->pending_action_requests.enqueue(std::unique_ptr<HlinkRequestFrame>(this->createPowerControlRequest_(false)));
                    break;
                case climate::ClimateMode::CLIMATE_MODE_COOL:
                    break;
                case climate::ClimateMode::CLIMATE_MODE_HEAT:
                    break;
                case climate::ClimateMode::CLIMATE_MODE_DRY:
                    break;
                default:
                    break;
                }
            }
        }

        esphome::climate::ClimateTraits HlinkAc::traits()
        {
            climate::ClimateTraits traits = climate::ClimateTraits();
            traits.set_supported_modes({climate::CLIMATE_MODE_OFF,
                                        climate::CLIMATE_MODE_COOL,
                                        climate::CLIMATE_MODE_HEAT,
                                        climate::CLIMATE_MODE_DRY});
            traits.set_supported_fan_modes({climate::CLIMATE_FAN_AUTO,
                                            climate::CLIMATE_FAN_LOW,
                                            climate::CLIMATE_FAN_MEDIUM,
                                            climate::CLIMATE_FAN_HIGH,
                                            climate::CLIMATE_FAN_QUIET});
            traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF,
                                              climate::CLIMATE_SWING_VERTICAL});
            traits.set_visual_min_temperature(16.0f);
            traits.set_visual_max_temperature(32.0f);
            traits.set_supports_current_temperature(true);
            traits.set_visual_target_temperature_step(1.0f);
            traits.set_visual_current_temperature_step(1.0f);
            return traits;
        }

        int8_t CircularRequestsQueue::enqueue(std::unique_ptr<HlinkRequestFrame> request)
        {
            if (this->is_full())
            {
                ESP_LOGE(TAG, "Action requests queue is full");
                return -1;
            }
            else if (this->is_empty())
            {
                front_++;
            }
            rear_ = (rear_ + 1) % REQUESTS_QUEUE_SIZE;
            requests_[rear_] = std::move(request); // Transfer ownership using std::move
            size_++;
            return 1;
        }

        std::unique_ptr<HlinkRequestFrame> CircularRequestsQueue::dequeue()
        {
            if (this->is_empty())
                return nullptr;
            std::unique_ptr<HlinkRequestFrame> dequeued_request = std::move(requests_[front_]);
            if (front_ == rear_)
            {
                front_ = -1;
                rear_ = -1;
            }
            else
            {
                front_ = (front_ + 1) % REQUESTS_QUEUE_SIZE;
            }
            size_--;

            return dequeued_request;
        }

        bool CircularRequestsQueue::is_empty() { return front_ == -1; }

        bool CircularRequestsQueue::is_full() { return (rear_ + 1) % REQUESTS_QUEUE_SIZE == front_; }

        uint8_t CircularRequestsQueue::size(){ return size_; }

        HlinkRequestFrame *HlinkAc::createPowerControlRequest_(bool is_on)
        {
            return new HlinkRequestFrame{HlinkRequestFrame::Type::ST, {0x0000, is_on ? 0x0001 : 0x0000, HlinkRequestFrame::AttributeFormat::FOUR_DIGITS}};
        }
    }
}