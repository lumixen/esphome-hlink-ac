#pragma once
#include <queue>

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/climate/climate.h"

namespace esphome
{
  namespace hlink_ac
  {
    enum HlinkComponentState : uint8_t
    {
      IDLE,
      REQUEST_NEXT_FEATURE,
      READ_NEXT_FEATURE,
      PUBLISH_CLIMATE_UPDATE_IF_ANY,
      APPLY_REQUEST,
      ACK_APPLIED_REQUEST
    };

    struct HvacStatus
    {
      optional<bool> power_state;
      optional<float> current_temperature;
      optional<float> target_temperature;
      optional<esphome::climate::ClimateMode> mode;
      optional<esphome::climate::ClimateFanMode> fan_mode;
      optional<esphome::climate::ClimateSwingMode> swing_mode;
      bool ready()
      {
        return power_state.has_value() && current_temperature.has_value() && target_temperature.has_value() && mode.has_value() && fan_mode.has_value() && swing_mode.has_value();
      }
    };

    enum FeatureType : uint16_t
    {
      POWER_STATE = 0x0000,
      MODE = 0x0001,
      TARGET_TEMP = 0x0003,
      CURRENT_TEMP = 0x0100,
      SWING_MODE = 0x0014,
      FAN_MODE = 0x0002
    };

    struct HlinkRequestFrame
    {
      enum class Type
      {
        MT,
        ST
      };
      enum class AttributeFormat
      {
        TWO_DIGITS,
        FOUR_DIGITS
      };
      struct ProgramPayload
      {
        uint16_t first;
        optional<uint16_t> secondary;
        optional<AttributeFormat> secondary_format;
      };
      Type type;
      ProgramPayload p;
    };
    struct HlinkResponseFrame
    {
      enum class Status
      {
        PROCESSING,
        OK,
        ACK_OK,
        NG,
        INVALID
      };
      Status status;
      uint32_t p_value;
      uint16_t checksum;
    };

    struct ComponentStatus
    {
      HlinkComponentState state = IDLE;
      uint32_t timeout_counter_started_at_ms = 0;
      uint32_t non_idle_timeout_limit_ms = 0;
      uint8_t requested_feature = 0;
      uint32_t last_frame_sent_at_ms = 0;
      uint8_t requests_left_to_apply = 0;

      void refresh_non_idle_timeout(uint32_t non_idle_timeout_limit_ms) {
        this->timeout_counter_started_at_ms = millis();
        this->non_idle_timeout_limit_ms = non_idle_timeout_limit_ms;
      }

      bool reached_timeout_thereshold() {
        return millis() - timeout_counter_started_at_ms > non_idle_timeout_limit_ms;
      }

      bool can_send_next_frame() {
        return millis() - last_frame_sent_at_ms > 20;
      }

      void reset_state() {
        state = IDLE;
        timeout_counter_started_at_ms = 0;
        non_idle_timeout_limit_ms = 0;
        last_frame_sent_at_ms = 0;
        requested_feature = 0;
        requests_left_to_apply = 0;
      }
    };

    static const uint8_t REQUESTS_QUEUE_SIZE = 16;
    class CircularRequestsQueue {
      public:
       int8_t enqueue(std::unique_ptr<HlinkRequestFrame> request);
       std::unique_ptr<HlinkRequestFrame> dequeue();
       bool is_empty();
       bool is_full();
       uint8_t size();
     
      protected:
       int front_{-1};
       int rear_{-1};
       uint8_t size_{0};
       std::unique_ptr<HlinkRequestFrame> requests_[REQUESTS_QUEUE_SIZE];
     };

    class HlinkAc : public Component, public uart::UARTDevice, public climate::Climate
    {
    public:
      // Component overrides
      void setup() override;
      void loop() override;
      void dump_config() override;
      // Climate overrides
      void control(const esphome::climate::ClimateCall &call) override;
      esphome::climate::ClimateTraits traits() override;

    protected:
      ComponentStatus status_ = ComponentStatus();
      HvacStatus hvac_status_ = HvacStatus();
      CircularRequestsQueue pending_action_requests;
      void request_status_update_();
      void write_cmd_request_(FeatureType feature_type);
      void write_hlink_frame_(HlinkRequestFrame frame);
      void capture_feature_response_to_hvac_status_(FeatureType requested_feature, HlinkResponseFrame feature_response);
      void publish_climate_update_if_any_();
      HlinkResponseFrame read_cmd_response_(uint32_t timeout_ms);
      std::unique_ptr<HlinkRequestFrame> createRequestFrame_(uint16_t primary_control, uint16_t secondary_control, optional<HlinkRequestFrame::AttributeFormat> secondary_control_format = HlinkRequestFrame::AttributeFormat::TWO_DIGITS);
    };
  }
}