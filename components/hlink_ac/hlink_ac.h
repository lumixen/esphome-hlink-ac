#pragma once

// #include "esphome/components/climate/climate.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome
{
  namespace hlink_ac
  {

    struct DeviceStatus
    {
      optional<float> target_temperature;
      // esphome::optional<esphome::climate::ClimateMode> mode;
      // esphome::optional<esphome::climate::ClimateFanMode> fan_mode;
      // esphome::optional<esphome::climate::ClimateSwingMode> swing_mode;
      // bool *power_state = nullptr;
      // uint16_t *target_temperature = nullptr;
      // climate::ClimateSwingMode swing_mode,
    };

    enum FeatureType : uint16_t
    {
      POWER_STATE = 0x0000,
      MODE = 0x0001,
      TARGET_TEMP = 0x0003,
      SWING_MODE = 0x0014,
      FAN_MODE = 0x0002
    };

    enum HlinkComponentState : uint8_t
    {
      IDLE,
      REQUEST_NEXT_FEATURE,
      READ_NEXT_FEATURE,
      APPLY_CONTROLS,
      PUBLISH_CLIMATE_UPDATE
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

    class HlinkAc : public Component, public uart::UARTDevice
    {
    public:
      void setup() override;
      void loop() override;
      void dump_config() override;

    protected:
      ComponentStatus status_ = ComponentStatus();
      DeviceStatus device_status_ = DeviceStatus();
      void request_status_update_();
      void write_cmd_request_(FeatureType feature_type);
      void write_hlink_frame_(HlinkRequestFrame frame);
      HlinkResponseFrame read_cmd_response_(uint32_t timeout_ms);
      void test_st_();
    };
  }
}