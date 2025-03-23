#pragma once

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
      optional<uint64_t> device_sn;
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
      ROOM_TEMP = 0x0100,
      SWING_MODE = 0x0014,
      FAN_MODE = 0x0002,
      DEVICE_SN = 0x0900
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
      uint64_t p_value;
      uint16_t checksum;
    };

    struct ComponentStatus
    {
      HlinkComponentState state = IDLE;
      uint32_t status_changed_at_ms = 0;
      int8_t requested_feature = 0;
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
      void request_status_update_();
      void write_cmd_request_(FeatureType feature_type);
      void write_hlink_frame_(HlinkRequestFrame frame);
      void capture_feature_response_to_hvac_status_(FeatureType requested_feature, HlinkResponseFrame feature_response);
      void publish_climate_update_if_needed_();
      HlinkResponseFrame read_cmd_response_(uint32_t timeout_ms);
      void test_st_();
    };
  }
}