#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/climate/climate.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
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
      optional<std::string> model_name;
#ifdef USE_SWITCH
      optional<bool> remote_control_lock;
#endif
      bool has_hvac_status()
      {
        return power_state.has_value() && current_temperature.has_value() && target_temperature.has_value() && mode.has_value() && fan_mode.has_value() && swing_mode.has_value();
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
      CURRENT_INDOOR_TEMP = 0x0100,
      CURRENT_OUTDOOR_TEMP = 0x0102, // Available only when unit is working, otherwise might return 7E value
      ACTIVITY_STATUS = 0x0301,      // 0000=Stand-by FFFF=Active
      BEEPER = 0x0800, // Write-only
      MODEL_NAME = 0x0900,
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

    constexpr uint16_t HLINK_BEEPER_ON = 0x0007;
    constexpr uint16_t HLINK_BEEPER_OFF = 0x0006;

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
      optional<std::vector<uint8_t>> p_value;
      uint16_t checksum;

      optional<uint16_t> p_value_as_uint16() const
      {
        if (!p_value.has_value() || p_value->empty())
        {
          return {};
        }
        if (p_value->size() == 1)
        {
          return static_cast<uint16_t>((*p_value)[0]);
        }
        return (static_cast<uint16_t>((*p_value)[0]) << 8) | static_cast<uint16_t>((*p_value)[1]);
      }

      optional<int8_t> p_value_as_int8() const
      {
        if (!p_value.has_value() || p_value->size() != 1)
        {
          return {};
        }
        return static_cast<int8_t>((*p_value)[0]);
      }
    };

    // Polled AC status features
    constexpr FeatureType features[] = {
        POWER_STATE,
        MODE,
        TARGET_TEMP,
        CURRENT_INDOOR_TEMP,
        FAN_MODE,
        SWING_MODE,
        MODEL_NAME
#ifdef USE_SWITCH
        ,
        REMOTE_CONTROL_LOCK
#endif
#ifdef USE_SENSOR
        ,
        CURRENT_OUTDOOR_TEMP
#endif
    };
    constexpr int features_size = sizeof(features) / sizeof(features[0]);

    struct ComponentStatus
    {
      HlinkComponentState state = IDLE;
      uint32_t timeout_counter_started_at_ms = 0;
      uint32_t non_idle_timeout_limit_ms = 0;
      uint8_t requested_read_feature_index = 0;
      uint32_t last_frame_sent_at_ms = 0;
      uint8_t requests_left_to_apply = 0;
      std::unique_ptr<HlinkRequestFrame> currently_applying_message = nullptr;

      void refresh_non_idle_timeout(uint32_t non_idle_timeout_limit_ms)
      {
        this->timeout_counter_started_at_ms = millis();
        this->non_idle_timeout_limit_ms = non_idle_timeout_limit_ms;
      }

      bool reached_timeout_thereshold()
      {
        return millis() - timeout_counter_started_at_ms > non_idle_timeout_limit_ms;
      }

      bool can_send_next_frame()
      {
        // Min interval between frames shouldn't be less than MIN_INTERVAL_BETWEEN_REQUESTS ms or AC will return NG
        return millis() - last_frame_sent_at_ms > MIN_INTERVAL_BETWEEN_REQUESTS;
      }

      FeatureType get_requested_read_feature()
      {
        return features[requested_read_feature_index];
      }

      void read_next_feature_if_any()
      {
        if (requested_read_feature_index + 1 < features_size)
        {
          state = REQUEST_NEXT_FEATURE;
          requested_read_feature_index++;
        }
        else
        {
          state = PUBLISH_CLIMATE_UPDATE_IF_ANY;
        }
      }

      void reset_state()
      {
        state = IDLE;
        timeout_counter_started_at_ms = 0;
        non_idle_timeout_limit_ms = 0;
        last_frame_sent_at_ms = 0;
        requested_read_feature_index = 0;
        requests_left_to_apply = 0;
        currently_applying_message = nullptr;
      }
    };

#ifdef USE_SENSOR
    enum class SensorType
    {
      OUTDOOR_TEMPERATURE = 0,
      // Used to count the number of sensors in the enum
      COUNT,
    };
#endif

    static const uint8_t REQUESTS_QUEUE_SIZE = 16;
    class CircularRequestsQueue
    {
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
      void set_beeper_switch(switch_::Switch *sw);
      void enqueue_remote_lock_action(bool state);
      void enqueue_beeper_state_action(bool state);

    protected:
      switch_::Switch *remote_lock_switch_{nullptr};
      switch_::Switch *beeper_switch_{nullptr};
#endif
#ifdef USE_SENSOR
    public:
      void set_sensor(SensorType type, sensor::Sensor *s);

    protected:
      void update_sensor_state_(SensorType type, float value);
      sensor::Sensor *sensors_[(size_t)SensorType::COUNT]{nullptr};
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
      void handle_feature_read_response_(FeatureType requested_feature, HlinkResponseFrame response);
      void handle_feature_write_response_ack_(HlinkRequestFrame applied_request);
      void publish_updates_if_any_();
      HlinkResponseFrame read_hlink_frame_(uint32_t timeout_ms);
      void write_hlink_frame_(HlinkRequestFrame frame);
      std::unique_ptr<HlinkRequestFrame> createRequestFrame_(uint16_t primary_control, uint16_t secondary_control, optional<HlinkRequestFrame::AttributeFormat> secondary_control_format = HlinkRequestFrame::AttributeFormat::TWO_DIGITS);
    };
  }
}