#include "esphome/core/log.h"
#include "hlink_ac.h"

namespace esphome {
namespace hlink_ac {
static const char *const TAG = "hlink_ac";

const HlinkResponseFrame HLINK_RESPONSE_NOTHING = {HlinkResponseFrame::Status::NOTHING};
const HlinkResponseFrame HLINK_RESPONSE_INVALID = {HlinkResponseFrame::Status::INVALID};
const HlinkResponseFrame HLINK_RESPONSE_ACK_OK = {HlinkResponseFrame::Status::OK};

void HlinkAc::setup() {
  // Setup default polling features
  this->status_.polling_features.push_back(
      {{HlinkRequestFrame::Type::MT, {FeatureType::POWER_STATE}}, [this](const HlinkResponseFrame &response) {
         this->hlink_entity_status_.power_state = response.p_value_as_uint16();
       }});
  this->status_.polling_features.push_back(
      {{HlinkRequestFrame::Type::MT, {FeatureType::MODE}}, [this](const HlinkResponseFrame &response) {
         if (!this->hlink_entity_status_.power_state.has_value()) {
           ESP_LOGW(TAG, "Can't handle climate mode response without power state data");
           return;
         }
         this->hlink_entity_status_.hlink_climate_mode = response.p_value_as_uint16();
         if (!this->hlink_entity_status_.power_state.value()) {
           // Climate mode should be off when device is turned off
           this->hlink_entity_status_.mode = esphome::climate::ClimateMode::CLIMATE_MODE_OFF;
           return;
         }
         if (this->hlink_entity_status_.hlink_climate_mode == HLINK_MODE_HEAT) {
           this->hlink_entity_status_.mode = esphome::climate::ClimateMode::CLIMATE_MODE_HEAT;
         } else if (this->hlink_entity_status_.hlink_climate_mode == HLINK_MODE_COOL) {
           this->hlink_entity_status_.mode = esphome::climate::ClimateMode::CLIMATE_MODE_COOL;
         } else if (this->hlink_entity_status_.hlink_climate_mode == HLINK_MODE_DRY) {
           this->hlink_entity_status_.mode = esphome::climate::ClimateMode::CLIMATE_MODE_DRY;
         } else if (this->hlink_entity_status_.hlink_climate_mode == HLINK_MODE_FAN) {
           this->hlink_entity_status_.mode = esphome::climate::ClimateMode::CLIMATE_MODE_FAN_ONLY;
         } else if (this->hlink_entity_status_.hlink_climate_mode == HLINK_MODE_HEAT_AUTO) {
           this->hlink_entity_status_.mode = esphome::climate::ClimateMode::CLIMATE_MODE_AUTO;
         } else if (this->hlink_entity_status_.hlink_climate_mode == HLINK_MODE_COOL_AUTO) {
           this->hlink_entity_status_.mode = esphome::climate::ClimateMode::CLIMATE_MODE_AUTO;
         } else if (this->hlink_entity_status_.hlink_climate_mode == HLINK_MODE_DRY_AUTO) {
           this->hlink_entity_status_.mode = esphome::climate::ClimateMode::CLIMATE_MODE_AUTO;
         } else if (this->hlink_entity_status_.hlink_climate_mode == HLINK_MODE_AUTO) {
           this->hlink_entity_status_.mode = esphome::climate::ClimateMode::CLIMATE_MODE_AUTO;
         }
       }});
  this->status_.polling_features.push_back(
      {{HlinkRequestFrame::Type::MT, {FeatureType::TARGET_TEMP}}, [this](const HlinkResponseFrame &response) {
         if (response.p_value_as_uint16().has_value()) {
           uint16_t target_temperature = response.p_value_as_uint16().value();
           if ((this->hlink_entity_status_.hlink_climate_mode == HLINK_MODE_HEAT_AUTO ||
                this->hlink_entity_status_.hlink_climate_mode == HLINK_MODE_COOL_AUTO) &&
               target_temperature >= 0xFF00) {
             this->hlink_entity_status_.target_temperature = NAN;
             // In auto mode the target temperature control is not available
             // Instead, AC expects temperature offset in range [-3;+3] C
             // AUTO HEATING: FFFD -> FFFF, FFFE -> FF00, FFFF -> FF01, FF00 -> FF02, FF01 -> FF03, FF02 -> FF04, FF03
             // -> FF05
             // AUTO COOLING: FFFD -> FFFB, FFFE -> FFFC, FFFF -> FFFD, FF00 -> FFFE, FF01 -> FFFF, FF02 ->
             // FF00, FF03 -> FF01
             // Needs testing, it's not clear if offset makes any difference in real life
             int8_t offset_temp = static_cast<int8_t>(target_temperature - 0xFF00);
             if (this->hlink_entity_status_.hlink_climate_mode == HLINK_MODE_HEAT_AUTO) {
               this->hlink_entity_status_.target_temperature_auto_offset = offset_temp - 2;
             } else if (this->hlink_entity_status_.hlink_climate_mode == HLINK_MODE_COOL_AUTO) {
               this->hlink_entity_status_.target_temperature_auto_offset = offset_temp + 2;
             } else {
               this->hlink_entity_status_.target_temperature_auto_offset = {};
             }
           } else if (target_temperature >= PROTOCOL_TARGET_TEMP_MIN &&
                      target_temperature <= PROTOCOL_TARGET_TEMP_MAX) {
             this->hlink_entity_status_.target_temperature = target_temperature;
             this->hlink_entity_status_.target_temperature_auto_offset = {};
           } else {
             this->hlink_entity_status_.target_temperature = NAN;
             this->hlink_entity_status_.target_temperature_auto_offset = {};
           }
         }
       }});
  this->status_.polling_features.push_back(
      {{HlinkRequestFrame::Type::MT, {FeatureType::CURRENT_INDOOR_TEMP}}, [this](const HlinkResponseFrame &response) {
         this->hlink_entity_status_.current_temperature = response.p_value_as_uint16();
       }});
  this->status_.polling_features.push_back(
      {{HlinkRequestFrame::Type::MT, {FeatureType::SWING_MODE}}, [this](const HlinkResponseFrame &response) {
         if (response.p_value_as_uint16() == HLINK_SWING_OFF) {
           this->hlink_entity_status_.swing_mode = esphome::climate::ClimateSwingMode::CLIMATE_SWING_OFF;
         } else if (response.p_value_as_uint16() == HLINK_SWING_VERTICAL) {
           this->hlink_entity_status_.swing_mode = esphome::climate::ClimateSwingMode::CLIMATE_SWING_VERTICAL;
         }
       }});
  this->status_.polling_features.push_back(
      {{HlinkRequestFrame::Type::MT, {FeatureType::FAN_MODE}}, [this](const HlinkResponseFrame &response) {
         if (response.p_value_as_uint16() == HLINK_FAN_AUTO) {
           this->hlink_entity_status_.fan_mode = esphome::climate::ClimateFanMode::CLIMATE_FAN_AUTO;
         } else if (response.p_value_as_uint16() == HLINK_FAN_HIGH) {
           this->hlink_entity_status_.fan_mode = esphome::climate::ClimateFanMode::CLIMATE_FAN_HIGH;
         } else if (response.p_value_as_uint16() == HLINK_FAN_MEDIUM) {
           this->hlink_entity_status_.fan_mode = esphome::climate::ClimateFanMode::CLIMATE_FAN_MEDIUM;
         } else if (response.p_value_as_uint16() == HLINK_FAN_LOW) {
           this->hlink_entity_status_.fan_mode = esphome::climate::ClimateFanMode::CLIMATE_FAN_LOW;
         } else if (response.p_value_as_uint16() == HLINK_FAN_QUIET) {
           this->hlink_entity_status_.fan_mode = esphome::climate::ClimateFanMode::CLIMATE_FAN_QUIET;
         }
       }});

#ifdef USE_SWITCH
  // Restore beeper switch state from memory if available
  if (this->beeper_switch_ != nullptr) {
    auto beeper_switch_restored_state = this->beeper_switch_->get_initial_state_with_restore_mode();
    if (beeper_switch_restored_state.has_value() &&
        beeper_switch_restored_state.value() != this->beeper_switch_->state) {
      this->beeper_switch_->publish_state(beeper_switch_restored_state.value());
    }
  }
#endif
  ESP_LOGI(TAG, "Hlink AC component initialized.");
}

void HlinkAc::dump_config() {
  ESP_LOGCONFIG(TAG, "Hlink AC:");
  ESP_LOGCONFIG(TAG, "  Power state: %s",
                this->hlink_entity_status_.power_state.has_value()
                    ? this->hlink_entity_status_.power_state.value() ? "ON" : "OFF"
                    : "N/A");
  ESP_LOGCONFIG(TAG, "  Mode: %s",
                this->hlink_entity_status_.mode.has_value()
                    ? LOG_STR_ARG(climate_mode_to_string(this->hlink_entity_status_.mode.value()))
                    : "N/A");
  ESP_LOGCONFIG(TAG, "  Fan mode: %s",
                this->hlink_entity_status_.fan_mode.has_value()
                    ? LOG_STR_ARG(climate_fan_mode_to_string(this->hlink_entity_status_.fan_mode.value()))
                    : "N/A");
  ESP_LOGCONFIG(TAG, "  Swing mode: %s",
                this->hlink_entity_status_.swing_mode.has_value()
                    ? LOG_STR_ARG(climate_swing_mode_to_string(this->hlink_entity_status_.swing_mode.value()))
                    : "N/A");
  ESP_LOGCONFIG(
      TAG, "  Current temperature: %s",
      this->hlink_entity_status_.current_temperature.has_value()
          ? std::to_string(static_cast<int16_t>(this->hlink_entity_status_.current_temperature.value())).c_str()
          : "N/A");
  ESP_LOGCONFIG(
      TAG, "  Target temperature: %s",
      this->hlink_entity_status_.target_temperature.has_value() &&
              !std::isnan(this->hlink_entity_status_.target_temperature.value())
          ? std::to_string(static_cast<int16_t>(this->hlink_entity_status_.target_temperature.value())).c_str()
          : "N/A");
  ESP_LOGCONFIG(
      TAG, "  Auto target temperature offset: %s",
      this->hlink_entity_status_.target_temperature_auto_offset.has_value() &&
              !std::isnan(this->hlink_entity_status_.target_temperature_auto_offset.value())
          ? std::to_string(static_cast<int16_t>(this->hlink_entity_status_.target_temperature_auto_offset.value()))
                .c_str()
          : "N/A");
  ESP_LOGCONFIG(TAG, "  Model: %s",
                this->hlink_entity_status_.model_name.has_value()
                    ? this->hlink_entity_status_.model_name.value().c_str()
                    : "N/A");
#ifdef USE_SWITCH
  ESP_LOGCONFIG(TAG, "  Remote lock: %s",
                this->hlink_entity_status_.remote_control_lock.has_value()
                    ? this->hlink_entity_status_.remote_control_lock.value() ? "ON" : "OFF"
                    : "N/A");
#endif
  this->check_uart_settings(9600, 1, uart::UART_CONFIG_PARITY_ODD, 8);
}

void HlinkAc::request_status_update_() {
  if (this->status_.state == IDLE) {
    // Launch update sequence
    this->status_.state = REQUEST_NEXT_STATUS_FEATURE;
    this->status_.requested_feature_index = 0;
    this->status_.refresh_non_idle_timeout(this->status_.polling_features.size() * 400);
  }
}

/*
 * Main loop implements a state machine with the following states:
 * 1. IDLE - does nothing.
 * 2. REQUEST_NEXT_STATUS_FEATURE - sends a request for the next status feature, the list of requested features is
 * stored in the polling_features list.
 * 3. REQUEST_LOW_PRIORITY_FEATURE - sends a request for the low priority feature if any.
 * 4. READ_FEATURE_RESPONSE - reads a response for the requested hlink feature.
 * 5. PUBLISH_UPDATE_IF_ANY - once all features are read, updates components if there are any changes.
 * 6. APPLY_REQUEST - applies the requested climate controls from the queue.
 * 7. ACK_APPLIED_REQUEST - confirms successful applied control reqeuest.
 */
void HlinkAc::loop() {
  if (this->status_.state == REQUEST_NEXT_STATUS_FEATURE && this->status_.can_send_next_frame()) {
    HlinkRequest state_feature_request = this->status_.get_currently_polling_feature();
    this->status_.current_request = make_unique<HlinkRequest>(state_feature_request);
    this->write_hlink_frame_(state_feature_request.request_frame);
    this->status_.state = READ_FEATURE_RESPONSE;
  }

  if (this->status_.state == REQUEST_LOW_PRIORITY_FEATURE && this->status_.can_send_next_frame()) {
    if (this->status_.low_priority_hlink_request.has_value()) {
      HlinkRequest low_priority_feature_request = this->status_.low_priority_hlink_request.value();
      this->write_hlink_frame_(low_priority_feature_request.request_frame);
      this->status_.current_request = make_unique<HlinkRequest>(low_priority_feature_request);
      this->status_.low_priority_hlink_request = {};
      this->status_.state = READ_FEATURE_RESPONSE;
    }
  }

  if (this->status_.state == READ_FEATURE_RESPONSE) {
    HlinkResponseFrame response = this->read_hlink_frame_(50);
    if (this->status_.current_request == nullptr) {
      ESP_LOGW(TAG, "Received response for unknown feature");
      this->status_.reset_state();
      return;
    }
    HlinkRequest requested_feature = *this->status_.current_request;
    if (response.status != HlinkResponseFrame::Status::NOTHING) {
      this->handle_hlink_request_response_(requested_feature, response);
      if (this->status_.requested_feature_index == -1) {
        this->status_.state = IDLE;
      } else if (this->status_.requested_feature_index + 1 < this->status_.polling_features.size()) {
        this->status_.state = REQUEST_NEXT_STATUS_FEATURE;
        this->status_.requested_feature_index++;
      } else {
        this->status_.state = PUBLISH_UPDATE_IF_ANY;
        this->status_.requested_feature_index = -1;
        this->status_.last_status_polling_finished_at_ms = millis();
      }
      this->status_.current_request = nullptr;
    }
  }

  if (this->status_.state == PUBLISH_UPDATE_IF_ANY) {
    this->publish_updates_if_any_();
    this->status_.state = IDLE;
    return;
  }

  if (this->status_.state == APPLY_REQUEST && this->status_.can_send_next_frame()) {
    if (this->status_.requests_left_to_apply > 0) {
      std::unique_ptr<HlinkRequest> request_msg = this->pending_action_requests.dequeue();
      if (request_msg != nullptr) {
        this->write_hlink_frame_(request_msg->request_frame);
        this->status_.current_request = std::move(request_msg);
        this->status_.requests_left_to_apply--;
        this->status_.state = ACK_APPLIED_REQUEST;
      } else {
        this->status_.state = IDLE;
      }
    } else {
      this->status_.state = IDLE;
    }
  }

  if (this->status_.state == ACK_APPLIED_REQUEST) {
    HlinkResponseFrame response = this->read_hlink_frame_(50);
    if (response.status != HlinkResponseFrame::Status::NOTHING) {
      this->handle_hlink_request_response_(*this->status_.current_request, response);
      if (this->status_.requests_left_to_apply > 0) {
        this->status_.state = APPLY_REQUEST;
      } else {
        this->status_.state = IDLE;
      }
      this->status_.current_request = nullptr;
    }
    // Update status right away after the applied batch
    if (this->status_.state == IDLE) {
      this->request_status_update_();
    }
  }

  // Reset status to IDLE if we reached timeout deadline
  if (this->status_.state != IDLE && this->status_.reached_timeout_thereshold()) {
    ESP_LOGW(TAG, "Reached timeout while performing [%d] state action. Reset state to IDLE.", this->status_.state);
    if (this->status_.current_request != nullptr) {
      auto timeout_callback = this->status_.current_request->timeout_callback;
      if (timeout_callback != nullptr) {
        timeout_callback();
      }
    }
    this->status_.reset_state();
  }

  // If there are any pending requests - apply them ASAP
  if ((this->status_.state == IDLE || this->status_.state == REQUEST_NEXT_STATUS_FEATURE) &&
      this->pending_action_requests.size() > 0) {
    this->status_.reset_state();
#ifdef USE_SWITCH
    // Makes beep sound if beeper switch is available and turned on
    if (this->beeper_switch_ != nullptr && this->beeper_switch_->state) {
      this->pending_action_requests.enqueue(this->create_st_request_(FeatureType::BEEPER, HLINK_BEEP_ACTION));
    }
#endif
    this->status_.requests_left_to_apply = this->pending_action_requests.size();
    this->status_.state = APPLY_REQUEST;
    this->status_.refresh_non_idle_timeout(2000);
  }

  // Start polling cycle if we are in IDLE state and the status update interval is reached
  if (this->status_.state == IDLE &&
      this->status_.last_status_polling_finished_at_ms + STATUS_UPDATE_INTERVAL < millis()) {
    this->request_status_update_();
  }

  // Request low priority feature if idling and nothing else to do
  if (this->status_.state == IDLE && this->status_.low_priority_hlink_request.has_value()) {
    this->status_.state = REQUEST_LOW_PRIORITY_FEATURE;
    this->status_.refresh_non_idle_timeout(300);
  }
}

void HlinkAc::handle_hlink_request_response_(const HlinkRequest &request, const HlinkResponseFrame &response) {
  switch (response.status) {
    case HlinkResponseFrame::Status::OK:
      if (request.ok_callback != nullptr) {
        request.ok_callback(response);
      }
      break;
    case HlinkResponseFrame::Status::NG:
      ESP_LOGW(TAG, "Received NG response for [%s - %04X]",
               request.request_frame.type == HlinkRequestFrame::Type::MT ? "MT" : "ST",
               request.request_frame.p.address);
      if (request.ng_callback != nullptr) {
        request.ng_callback();
      }
      break;
    case HlinkResponseFrame::Status::INVALID:
      if (request.invalid_callback != nullptr) {
        request.invalid_callback();
      }
      ESP_LOGW(TAG, "Received INVALID response for [%s - %04X]",
               request.request_frame.type == HlinkRequestFrame::Type::MT ? "MT" : "ST",
               request.request_frame.p.address);
      break;
  }
}

void HlinkAc::publish_updates_if_any_() {
  if (this->hlink_entity_status_.has_hvac_status()) {
    bool should_publish_climate_state = false;
    if (!is_nanable_equal(this->target_temperature, this->hlink_entity_status_.target_temperature.value())) {
      this->target_temperature = this->hlink_entity_status_.target_temperature.value();
      should_publish_climate_state = true;
    }
    if (this->current_temperature != this->hlink_entity_status_.current_temperature.value()) {
      this->current_temperature = this->hlink_entity_status_.current_temperature.value();
      should_publish_climate_state = true;
    }
    if (this->mode != this->hlink_entity_status_.mode) {
      this->mode = this->hlink_entity_status_.mode.value();
      should_publish_climate_state = true;
    }
    if (this->fan_mode != this->hlink_entity_status_.fan_mode.value()) {
      this->fan_mode = this->hlink_entity_status_.fan_mode.value();
      should_publish_climate_state = true;
    }
    if (this->swing_mode != this->hlink_entity_status_.swing_mode.value()) {
      this->swing_mode = this->hlink_entity_status_.swing_mode.value();
      should_publish_climate_state = true;
    }
    if (should_publish_climate_state) {
      this->publish_state();
    }
  }
#ifdef USE_SWITCH
  if (this->remote_lock_switch_ != nullptr && this->hlink_entity_status_.remote_control_lock.has_value() &&
      this->remote_lock_switch_->state != this->hlink_entity_status_.remote_control_lock.value()) {
    this->remote_lock_switch_->publish_state(this->hlink_entity_status_.remote_control_lock.value());
  }
#endif
#ifdef USE_TEXT_SENSOR
  if (this->model_name_text_sensor_ != nullptr && this->hlink_entity_status_.model_name.has_value() &&
      this->model_name_text_sensor_->state != this->hlink_entity_status_.model_name.value()) {
    this->model_name_text_sensor_->publish_state(this->hlink_entity_status_.model_name.value());
  }
#endif
#ifdef USE_NUMBER
  if (this->temperature_offset_number_ != nullptr) {
    if (!is_nanable_equal(this->temperature_offset_number_->state,
                          this->hlink_entity_status_.target_temperature_auto_offset.value_or(0.0f))) {
      this->temperature_offset_number_->publish_state(
          this->hlink_entity_status_.target_temperature_auto_offset.value());
    }
  }
#endif
}

void HlinkAc::write_hlink_frame_(HlinkRequestFrame frame) {
  // Reset uart buffer before sending new frame
  while (this->available()) {
    this->read();
  }
  const char *message_type = frame.type == HlinkRequestFrame::Type::MT ? "MT" : "ST";
  uint8_t message_size = 17;  // Default message, e.g. "MT P=1234 C=1234\r"
  if (frame.p.data.has_value() && frame.p.data_format.value() == HlinkRequestFrame::AttributeFormat::TWO_DIGITS) {
    message_size = 20;  // "ST P=1234,12 C=1234\r"
  } else if (frame.p.data.has_value() &&
             frame.p.data_format.value() == HlinkRequestFrame::AttributeFormat::FOUR_DIGITS) {
    message_size = 22;  // "ST P=1234,1234 C=1234\r"
  }
  std::string message(message_size, 0x00);
  uint16_t checksum = ((frame.p.address >> 8) + (frame.p.address & 0xFF) + (frame.p.data.value_or(0) >> 8) +
                       (frame.p.data.value_or(0) & 0xFF)) ^
                      0xFFFF;
  if (message_size == 17) {
    sprintf(&message[0], "%s P=%04X C=%04X\x0D", message_type, frame.p.address, checksum);
  } else if (message_size == 20) {
    sprintf(&message[0], "%s P=%04X,%02X C=%04X\x0D", message_type, frame.p.address, frame.p.data.value(), checksum);
  } else if (message_size == 22) {
    sprintf(&message[0], "%s P=%04X,%04X C=%04X\x0D", message_type, frame.p.address, frame.p.data.value(), checksum);
  }
  // Send the message to uart
  this->write_str(message.c_str());
}

// Returns NOTHING state if nothing available on UART input yet
HlinkResponseFrame HlinkAc::read_hlink_frame_(uint32_t timeout_ms) {
  if (this->available()) {
    uint32_t started_millis = millis();
    std::string response_buf(HLINK_MSG_READ_BUFFER_SIZE, '\0');
    int read_index = 0;
    // Read response unless carriage return symbol, timeout or reasonable buffer size
    while (millis() - started_millis < timeout_ms || read_index < HLINK_MSG_READ_BUFFER_SIZE) {
      this->read_byte((uint8_t *) &response_buf[read_index]);
      if (response_buf[read_index] == HLINK_MSG_TERMINATION_SYMBOL) {
        break;
      }
      read_index++;
    }
    // Update the timestamp of the last frame received
    this->status_.last_frame_received_at_ms = millis();
    std::vector<std::string> response_tokens;
    for (int i = 0, last_space_i = 0; i <= read_index; i++) {
      if (response_buf[i] == ' ' || response_buf[i] == '\r') {
        uint8_t pos_shift = last_space_i > 0 ? 2 : 0;  // Shift ahead to remove 'X=' from the tokens after initial OK/NG
        response_tokens.push_back(response_buf.substr(last_space_i + pos_shift, i - last_space_i - pos_shift));
        last_space_i = i + 1;
      }
    }
    if (response_tokens.size() == 1 && response_tokens[0] == HLINK_MSG_OK_TOKEN) {
      // Ack frame
      return HLINK_RESPONSE_ACK_OK;
    }
    if (response_tokens.size() != 3) {
      ESP_LOGW(TAG, "Invalid H-link response: %s", response_buf.c_str());
      return HLINK_RESPONSE_INVALID;
    }

    HlinkResponseFrame::Status status;
    if (response_tokens[0] == HLINK_MSG_OK_TOKEN) {
      status = HlinkResponseFrame::Status::OK;
    } else if (response_tokens[0] == HLINK_MSG_NG_TOKEN) {
      status = HlinkResponseFrame::Status::NG;
    } else {
      ESP_LOGW(TAG, "Didn't understand first token. Response tokens array size: %d", response_tokens.size());
      for (int i = 0; i < response_tokens.size(); i++) {
        ESP_LOGW(TAG, "Token %d: %s", i, response_tokens[i].c_str());
      }
      return HLINK_RESPONSE_INVALID;
    }
    if (response_tokens[1].size() < 2 || response_tokens[1].size() % 2 != 0) {
      ESP_LOGW(TAG, "Invalid length for P= value: %s", response_tokens[1].c_str());
      return HLINK_RESPONSE_INVALID;
    }
    std::vector<uint8_t> p_value;
    for (size_t i = 0; i < response_tokens[1].size(); i += 2) {
      p_value.push_back(static_cast<uint8_t>(std::stoi(response_tokens[1].substr(i, 2), nullptr, 16)));
    }
    uint16_t checksum = std::stoi(response_tokens[2], nullptr, 16);
    return {status, p_value, checksum};
  }
  return HLINK_RESPONSE_NOTHING;
}

void HlinkAc::send_hlink_cmd(std::string address, std::string data) {
  if (address.size() != 4) {
    ESP_LOGW(TAG, "Invalid address length: %s", address.c_str());
    return;
  }
  if (data.size() != 4 && data.size() != 2) {
    ESP_LOGW(TAG, "Invalid data length: %s", data.c_str());
    return;
  }
  this->pending_action_requests.enqueue(this->create_st_request_(
      static_cast<uint16_t>(std::stoi(address, nullptr, 16)), static_cast<uint16_t>(std::stoi(data, nullptr, 16)),
      static_cast<HlinkRequestFrame::AttributeFormat>(data.size() == 4)));
}

void HlinkAc::control(const esphome::climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    climate::ClimateMode mode = *call.get_mode();
    uint16_t power_state = 0x0001;
    uint16_t h_link_mode = HLINK_MODE_AUTO;
    switch (mode) {
      case climate::ClimateMode::CLIMATE_MODE_OFF:
        power_state = 0x0000;
        break;
      case climate::ClimateMode::CLIMATE_MODE_COOL:
        h_link_mode = HLINK_MODE_COOL;
        break;
      case climate::ClimateMode::CLIMATE_MODE_HEAT:
        h_link_mode = HLINK_MODE_HEAT;
        break;
      case climate::ClimateMode::CLIMATE_MODE_DRY:
        h_link_mode = HLINK_MODE_DRY;
        break;
      case climate::ClimateMode::CLIMATE_MODE_FAN_ONLY:
        h_link_mode = HLINK_MODE_FAN;
        break;
      case climate::ClimateMode::CLIMATE_MODE_AUTO:
        h_link_mode = HLINK_MODE_AUTO;
        break;
      default:
        power_state = 0x0000;
        break;
    }
    this->pending_action_requests.enqueue(this->create_st_request_(FeatureType::POWER_STATE, power_state));
    this->pending_action_requests.enqueue(
        this->create_st_request_(FeatureType::MODE, h_link_mode, HlinkRequestFrame::AttributeFormat::FOUR_DIGITS,
                                 [this, power_state, mode](const HlinkResponseFrame &response) {
                                   this->hlink_entity_status_.power_state = power_state;
                                   this->hlink_entity_status_.mode = mode;
                                   this->mode = mode;
                                   this->publish_state();
                                 }));
  }
  if (call.get_fan_mode().has_value()) {
    climate::ClimateFanMode fan_mode = *call.get_fan_mode();
    uint16_t h_link_fan_speed = HLINK_FAN_AUTO;
    switch (fan_mode) {
      case climate::ClimateFanMode::CLIMATE_FAN_AUTO:
        h_link_fan_speed = HLINK_FAN_AUTO;
        break;
      case climate::ClimateFanMode::CLIMATE_FAN_HIGH:
        h_link_fan_speed = HLINK_FAN_HIGH;
        break;
      case climate::ClimateFanMode::CLIMATE_FAN_MEDIUM:
        h_link_fan_speed = HLINK_FAN_MEDIUM;
        break;
      case climate::ClimateFanMode::CLIMATE_FAN_LOW:
        h_link_fan_speed = HLINK_FAN_LOW;
        break;
      case climate::ClimateFanMode::CLIMATE_FAN_QUIET:
        h_link_fan_speed = HLINK_FAN_QUIET;
        break;
    }
    this->pending_action_requests.enqueue(this->create_st_request_(
        FeatureType::FAN_MODE, h_link_fan_speed, HlinkRequestFrame::AttributeFormat::TWO_DIGITS,
        [this, fan_mode](const HlinkResponseFrame &response) {
          this->hlink_entity_status_.fan_mode = fan_mode;
          this->fan_mode = fan_mode;
          this->publish_state();
        }));
  }
  if (call.get_target_temperature().has_value()) {
    float target_temperature = *call.get_target_temperature();
    this->pending_action_requests.enqueue(this->create_st_request_(
        FeatureType::TARGET_TEMP, target_temperature, HlinkRequestFrame::AttributeFormat::FOUR_DIGITS,
        [this, target_temperature](const HlinkResponseFrame &response) {
          this->hlink_entity_status_.target_temperature = target_temperature;
          this->target_temperature = target_temperature;
          this->publish_state();
        }));
  }
  if (call.get_swing_mode().has_value()) {
    climate::ClimateSwingMode swing_mode = *call.get_swing_mode();
    uint16_t h_link_swing_mode = HLINK_SWING_OFF;
    switch (swing_mode) {
      case climate::ClimateSwingMode::CLIMATE_SWING_OFF:
        h_link_swing_mode = HLINK_SWING_OFF;
        break;
      case climate::ClimateSwingMode::CLIMATE_SWING_VERTICAL:
        h_link_swing_mode = HLINK_SWING_VERTICAL;
        break;
    }
    this->pending_action_requests.enqueue(this->create_st_request_(
        FeatureType::SWING_MODE, h_link_swing_mode, HlinkRequestFrame::AttributeFormat::TWO_DIGITS,
        [this, swing_mode](const HlinkResponseFrame &response) {
          this->hlink_entity_status_.swing_mode = swing_mode;
          this->swing_mode = swing_mode;
          this->publish_state();
        }));
  }
}

void HlinkAc::set_supported_climate_modes(const std::set<climate::ClimateMode> &modes) {
  this->traits_.add_supported_mode(climate::CLIMATE_MODE_OFF);
  this->traits_.set_supported_modes(modes);
}

void HlinkAc::set_supported_swing_modes(const std::set<climate::ClimateSwingMode> &modes) {
  this->traits_.set_supported_swing_modes(modes);
}

void HlinkAc::set_supported_fan_modes(const std::set<climate::ClimateFanMode> &modes) {
  this->traits_.set_supported_fan_modes(modes);
}

esphome::climate::ClimateTraits HlinkAc::traits() {
  this->traits_.set_supports_current_temperature(true);
  return this->traits_;
}

#ifdef USE_SWITCH
void HlinkAc::set_remote_lock_switch(switch_::Switch *sw) {
  this->remote_lock_switch_ = sw;
  if (this->hlink_entity_status_.remote_control_lock.has_value()) {
    this->remote_lock_switch_->publish_state(this->hlink_entity_status_.remote_control_lock.value());
  }
  this->status_.polling_features.push_back({{HlinkRequestFrame::Type::MT, {FeatureType::REMOTE_CONTROL_LOCK}},
                                            [this, sw](const HlinkResponseFrame &response) {
                                              this->hlink_entity_status_.remote_control_lock =
                                                  response.p_value_as_uint16();
                                            }});
}

void HlinkAc::set_remote_lock_state(bool state) {
  auto publish_current_state = [this]() {
    this->remote_lock_switch_->publish_state(this->hlink_entity_status_.remote_control_lock.value());
  };
  this->pending_action_requests.enqueue(this->create_st_request_(
      FeatureType::REMOTE_CONTROL_LOCK, state, HlinkRequestFrame::AttributeFormat::TWO_DIGITS,
      [this, state](const HlinkResponseFrame &response) {
        this->hlink_entity_status_.remote_control_lock = state;
        this->remote_lock_switch_->publish_state(state);
      },
      publish_current_state, publish_current_state, publish_current_state));
}

void HlinkAc::set_beeper_switch(switch_::Switch *sw) { this->beeper_switch_ = sw; }

void HlinkAc::handle_beep_state_change(bool state) {
  if (state) {
    this->pending_action_requests.enqueue(this->create_st_request_(FeatureType::BEEPER, HLINK_BEEP_ACTION));
  }
}
#endif

#ifdef USE_SENSOR
void HlinkAc::set_sensor(SensorType type, sensor::Sensor *s) {
  switch (type) {
    case SensorType::OUTDOOR_TEMPERATURE:
      this->status_.polling_features.push_back({{HlinkRequestFrame::Type::MT, {FeatureType::CURRENT_OUTDOOR_TEMP}},
                                                [this, s](const HlinkResponseFrame &response) {
                                                  optional<int8_t> raw_sensor_value = response.p_value_as_int8();
                                                  float sensor_value =
                                                      (raw_sensor_value.has_value() && raw_sensor_value != 0x7E)
                                                          ? raw_sensor_value.value()
                                                          : NAN;
                                                  this->update_sensor_state_(s, sensor_value);
                                                }});
      break;
    default:
      break;
  }
}

void HlinkAc::update_sensor_state_(sensor::Sensor *sensor, float value) {
  if (sensor != nullptr) {
    float current_state = sensor->raw_state;
    if (is_nanable_equal(current_state, value)) {
      return;
    }
    sensor->publish_state(value);
  }
}
#endif
#ifdef USE_TEXT_SENSOR
void HlinkAc::set_text_sensor(TextSensorType type, text_sensor::TextSensor *text_sensor) {
  switch (type) {
    case TextSensorType::MODEL_NAME:
      this->model_name_text_sensor_ = text_sensor;
      this->status_.polling_features.push_back(
          {{HlinkRequestFrame::Type::MT, {FeatureType::MODEL_NAME}}, [this](const HlinkResponseFrame &response) {
             if (response.p_value.has_value()) {
               this->hlink_entity_status_.model_name = std::string(response.p_value->begin(), response.p_value->end());
             }
           }});
      break;
    default:
      break;
  }
}

void HlinkAc::set_debug_text_sensor(uint16_t address, text_sensor::TextSensor *text_sensor) {
  this->status_.polling_features.push_back(
      {{HlinkRequestFrame::Type::MT, {address}}, [text_sensor](const HlinkResponseFrame &response) {
         if (response.p_value.has_value()) {
           std::string response_value = response.p_value_as_string().value();
           if (text_sensor->state != response_value) {
             text_sensor->publish_state(response_value);
           }
         }
       }});
}

void HlinkAc::set_debug_discovery_text_sensor(text_sensor::TextSensor *text_sensor) {
  this->set_timeout(20000, [this, text_sensor]() {
    auto create_discovery_request = std::make_shared<std::function<HlinkRequest(uint16_t)>>();
    *create_discovery_request = [this, text_sensor, create_discovery_request](uint16_t address) {
      return HlinkRequest{
          HlinkRequestFrame{HlinkRequestFrame::Type::MT, {address}},
          [this, text_sensor, address, create_discovery_request](const HlinkResponseFrame &response) mutable {
            char address_str[5];
            sprintf(address_str, "%04X", address);
            std::string sensor_value = std::string(address_str) + ":" + response.p_value_as_string().value();
            text_sensor->publish_state(sensor_value);
            this->status_.low_priority_hlink_request = (*create_discovery_request)(address + 1);
          },
          [this, address, create_discovery_request]() mutable {
            this->status_.low_priority_hlink_request = (*create_discovery_request)(address + 1);
          },
          [this, address, create_discovery_request]() mutable {
            this->status_.low_priority_hlink_request = (*create_discovery_request)(address);
          },
          [this, address, create_discovery_request]() mutable {
            this->status_.low_priority_hlink_request = (*create_discovery_request)(address);
          }};
    };

    this->status_.low_priority_hlink_request = (*create_discovery_request)(0x0000);
  });
}
#endif
#ifdef USE_NUMBER
void HlinkAc::set_auto_temperature_offset(float offset) {
  uint16_t offset_temp = static_cast<uint8_t>(static_cast<int8_t>(offset)) + 0xFF00;
  auto publish_current_state = [this]() {
    this->temperature_offset_number_->publish_state(
        this->hlink_entity_status_.target_temperature_auto_offset.value_or(0.0f));
  };
  this->pending_action_requests.enqueue(this->create_st_request_(
      FeatureType::TARGET_TEMP, offset_temp, HlinkRequestFrame::AttributeFormat::FOUR_DIGITS,
      [this, offset](const HlinkResponseFrame &response) {
        this->hlink_entity_status_.target_temperature_auto_offset = offset;
        this->temperature_offset_number_->publish_state(offset);
      },
      publish_current_state, publish_current_state, publish_current_state));
}
#endif

int8_t CircularRequestsQueue::enqueue(std::unique_ptr<HlinkRequest> request) {
  if (this->is_full()) {
    ESP_LOGE(TAG, "Action requests queue is full");
    return -1;
  } else if (this->is_empty()) {
    front_++;
  }
  rear_ = (rear_ + 1) % REQUESTS_QUEUE_SIZE;
  requests_[rear_] = std::move(request);  // Transfer ownership using std::move
  size_++;
  return 1;
}

std::unique_ptr<HlinkRequest> CircularRequestsQueue::dequeue() {
  if (this->is_empty())
    return nullptr;
  std::unique_ptr<HlinkRequest> dequeued_request = std::move(requests_[front_]);
  if (front_ == rear_) {
    front_ = -1;
    rear_ = -1;
  } else {
    front_ = (front_ + 1) % REQUESTS_QUEUE_SIZE;
  }
  size_--;

  return dequeued_request;
}

bool CircularRequestsQueue::is_empty() { return front_ == -1; }

bool CircularRequestsQueue::is_full() { return (rear_ + 1) % REQUESTS_QUEUE_SIZE == front_; }

uint8_t CircularRequestsQueue::size() { return size_; }

std::unique_ptr<HlinkRequest> HlinkAc::create_st_request_(
    uint16_t address, uint16_t data, optional<HlinkRequestFrame::AttributeFormat> data_format,
    std::function<void(const HlinkResponseFrame &response)> ok_callback, std::function<void()> ng_callback,
    std::function<void()> invalid_callback, std::function<void()> timeout_callback) {
  {
    return std::unique_ptr<HlinkRequest>(
        new HlinkRequest{HlinkRequestFrame{HlinkRequestFrame::Type::ST, {address, data, data_format}}, ok_callback,
                         ng_callback, invalid_callback, timeout_callback});
  }
}
}  // namespace hlink_ac
}  // namespace esphome