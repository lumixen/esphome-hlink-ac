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
      APPLY_CONTROLS,
      PUBLISH_CLIMATE_UPDATE
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
      uint32_t status_changed_at_ms = 0;
      int8_t requested_feature = 0;
    };

    static const uint8_t REQUESTS_QUEUE_SIZE = 16;
    class CircularRequestsQueue {
      public:
       int8_t enqueue(std::unique_ptr<HlinkRequestFrame> request);
       std::unique_ptr<HlinkRequestFrame> dequeue();
       bool is_empty();
       bool is_full();
     
      protected:
       int front_{-1};
       int rear_{-1};
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
      void apply_requests_();
      void write_cmd_request_(FeatureType feature_type);
      std::string hlink_frame_request_to_string_(HlinkRequestFrame frame);
      void write_hlink_frame_(HlinkRequestFrame frame);
      void capture_feature_response_to_hvac_status_(FeatureType requested_feature, HlinkResponseFrame feature_response);
      void publish_climate_update_if_needed_();
      HlinkResponseFrame read_cmd_response_(uint32_t timeout_ms);
      HlinkRequestFrame* createPowerControlRequest_(bool is_on);
      void test_st_();
    };
  }
}