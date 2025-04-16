#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/climate/climate.h"

#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome
{
  namespace hlink_ac
  {
  
    constexpr uint32_t STATUS_UPDATE_INTERVAL = 5000;
    constexpr uint32_t STATUS_UPDATE_TIMEOUT = 2000;
    constexpr uint32_t MIN_INTERVAL_BETWEEN_REQUESTS = 60;

    constexpr uint8_t HLINK_MSG_READ_BUFFER_SIZE = 35;
    constexpr uint8_t HLINK_MSG_TERMINATION_SYMBOL = 0x0D;

    static const std::string HLINK_MSG_OK_TOKEN = "OK";
    static const std::string HLINK_MSG_NG_TOKEN = "NG";

    constexpr uint32_t MIN_TARGET_TEMPERATURE = 16;
    constexpr uint32_t MAX_TARGET_TEMPERATURE = 32;

    enum HlinkComponentState : uint8_t
    {
      IDLE,
      REQUEST_NEXT_FEATURE,
      READ_NEXT_FEATURE,
      PUBLISH_CLIMATE_UPDATE_IF_ANY,
      APPLY_REQUEST,
      ACK_APPLIED_REQUEST
    };

    struct HlinkEntityStatus
    {
      optional<bool> power_state;
      optional<float> current_temperature;
      optional<float> target_temperature;
      optional<esphome::climate::ClimateMode> mode;
      optional<esphome::climate::ClimateFanMode> fan_mode;
      optional<esphome::climate::ClimateSwingMode> swing_mode;
      #ifdef USE_SWITCH
      optional<bool> remote_control_lock;
      #endif
      bool has_hvac_status()
      {
        return power_state.has_value()
          && current_temperature.has_value()
          && target_temperature.has_value()
          && mode.has_value()
          && fan_mode.has_value()
          && swing_mode.has_value();
      }
    };

    enum FeatureType : uint16_t
    {
      POWER_STATE = 0x0000,
      MODE = 0x0001,
      FAN_MODE = 0x0002,
      TARGET_TEMP = 0x0003,
      REMOTE_CONTROL_LOCK = 0x0006,
      SWING_MODE = 0x0014,
      CURRENT_TEMP = 0x0100,
    };

    constexpr uint16_t HLINK_MODE_HEAT = 0x0010;
    constexpr uint16_t HLINK_MODE_HEAT_AUTO = 0x8010;
    constexpr uint16_t HLINK_MODE_COOL = 0x0040;
    constexpr uint16_t HLINK_MODE_COOL_AUTO = 0x8040;
    constexpr uint16_t HLINK_MODE_DRY = 0x0020;

    constexpr uint16_t HLINK_SWING_OFF = 0x0000;
    constexpr uint16_t HLINK_SWING_VERTICAL = 0x0001;

    constexpr uint16_t HLINK_FAN_AUTO = 0x0000;
    constexpr uint16_t HLINK_FAN_HIGH = 0x0001;
    constexpr uint16_t HLINK_FAN_MEDIUM = 0x0002;
    constexpr uint16_t HLINK_FAN_LOW = 0x0003;
    constexpr uint16_t HLINK_FAN_QUIET = 0x0004;

    constexpr uint16_t HLINK_REMOTE_LOCK_ON = 0x0001;
    constexpr uint16_t HLINK_REMOTE_LOCK_OFF = 0x0000;

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
        NOTHING,
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
        // Min interval between frames shouldn't be less than MIN_INTERVAL_BETWEEN_REQUESTS ms or AC will return NG
        return millis() - last_frame_sent_at_ms > MIN_INTERVAL_BETWEEN_REQUESTS;
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
      #ifdef USE_SWITCH
      public:
        void set_remote_lock_switch(switch_::Switch *sw);
        void enqueue_remote_lock_action(bool state);
      protected:
        switch_::Switch *remote_lock_switch_{nullptr};
      #endif
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
      HlinkEntityStatus hlink_entity_status_ = HlinkEntityStatus();
      CircularRequestsQueue pending_action_requests;
      void request_status_update_();
      void write_feature_status_request_(FeatureType feature_type);
      void apply_feature_response_to_hlink_entity_(FeatureType requested_feature, HlinkResponseFrame feature_response);
      void publish_updates_if_any_();
      HlinkResponseFrame read_hlink_frame_(uint32_t timeout_ms);
      void write_hlink_frame_(HlinkRequestFrame frame);
      std::unique_ptr<HlinkRequestFrame> createRequestFrame_(uint16_t primary_control, uint16_t secondary_control, optional<HlinkRequestFrame::AttributeFormat> secondary_control_format = HlinkRequestFrame::AttributeFormat::TWO_DIGITS);
    };
  }
}