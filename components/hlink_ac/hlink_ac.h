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
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace hlink_ac {

constexpr uint32_t STATUS_UPDATE_INTERVAL = 5000;
constexpr uint32_t STATUS_UPDATE_TIMEOUT = 2000;
constexpr uint32_t MIN_INTERVAL_BETWEEN_REQUESTS = 60;

constexpr uint8_t HLINK_MSG_READ_BUFFER_SIZE = 35;
constexpr uint8_t HLINK_MSG_TERMINATION_SYMBOL = 0x0D;

static const std::string HLINK_MSG_OK_TOKEN = "OK";
static const std::string HLINK_MSG_NG_TOKEN = "NG";

enum HlinkComponentState : uint8_t {
  IDLE,
  REQUEST_NEXT_STATUS_FEATURE,
  REQUEST_LOW_PRIORITY_FEATURE,
  READ_FEATURE_RESPONSE,
  PUBLISH_UPDATE_IF_ANY,
  APPLY_REQUEST,
  ACK_APPLIED_REQUEST
};

struct HlinkEntityStatus {
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
  bool has_hvac_status() {
    return power_state.has_value() && current_temperature.has_value() && target_temperature.has_value() &&
           mode.has_value();
  }
};

enum FeatureType : uint16_t {
  POWER_STATE = 0x0000,
  MODE = 0x0001,
  FAN_MODE = 0x0002,
  TARGET_TEMP = 0x0003,
  REMOTE_CONTROL_LOCK = 0x0006,
  SWING_MODE = 0x0014,
  CURRENT_INDOOR_TEMP = 0x0100,
  CURRENT_OUTDOOR_TEMP = 0x0102,  // Available only when unit is working, otherwise might return 7E value
  ACTIVITY_STATUS = 0x0301,       // 0000=Stand-by FFFF=Active
  BEEPER = 0x0800,                // Triggers beeper sound
  MODEL_NAME = 0x0900,
};

constexpr uint16_t HLINK_MODE_HEAT = 0x0010;
constexpr uint16_t HLINK_MODE_HEAT_AUTO = 0x8010;
constexpr uint16_t HLINK_MODE_COOL = 0x0040;
constexpr uint16_t HLINK_MODE_COOL_AUTO = 0x8040;
constexpr uint16_t HLINK_MODE_DRY = 0x0020;
constexpr uint16_t HLINK_MODE_FAN = 0x0050;
constexpr uint16_t HLINK_MODE_AUTO = 0x8000;

constexpr uint16_t HLINK_SWING_OFF = 0x0000;
constexpr uint16_t HLINK_SWING_VERTICAL = 0x0001;

constexpr uint16_t HLINK_FAN_AUTO = 0x0000;
constexpr uint16_t HLINK_FAN_HIGH = 0x0001;
constexpr uint16_t HLINK_FAN_MEDIUM = 0x0002;
constexpr uint16_t HLINK_FAN_LOW = 0x0003;
constexpr uint16_t HLINK_FAN_QUIET = 0x0004;

constexpr uint16_t HLINK_REMOTE_LOCK_ON = 0x0001;
constexpr uint16_t HLINK_REMOTE_LOCK_OFF = 0x0000;

constexpr uint16_t HLINK_BEEP_ACTION = 0x0007;

struct HlinkRequestFrame {
  enum class Type { MT, ST };
  enum class AttributeFormat { TWO_DIGITS, FOUR_DIGITS };
  struct ProgramPayload {
    uint16_t first;
    optional<uint16_t> secondary;
    optional<AttributeFormat> secondary_format;
  };
  Type type;
  ProgramPayload p;
};
struct HlinkResponseFrame {
  enum class Status { NOTHING, OK, ACK_OK, NG, INVALID };
  Status status;
  optional<std::vector<uint8_t>> p_value;
  uint16_t checksum;

  optional<uint16_t> p_value_as_uint16() const {
    if (!p_value.has_value() || p_value->empty()) {
      return {};
    }
    if (p_value->size() == 1) {
      return static_cast<uint16_t>((*p_value)[0]);
    }
    return (static_cast<uint16_t>((*p_value)[0]) << 8) | static_cast<uint16_t>((*p_value)[1]);
  }

  optional<int8_t> p_value_as_int8() const {
    if (!p_value.has_value() || p_value->size() != 1) {
      return {};
    }
    return static_cast<int8_t>((*p_value)[0]);
  }

  optional<std::string> p_value_as_string() const {
    if (!p_value.has_value()) {
      return {};
    }
    std::string hex_string;
    for (const auto &byte : *p_value) {
      char buffer[3];
      sprintf(buffer, "%02X", byte);
      hex_string += buffer;
    }
    return hex_string;
  }
};

struct HlinkFeatureRequest {
  HlinkRequestFrame request_frame;
  std::function<void(const HlinkResponseFrame &response)> ok_callback;
  std::function<void()> ng_callback;
  std::function<void()> invalid_callback;
  std::function<void()> timeout_callback;
};

struct ComponentStatus {
  HlinkComponentState state = IDLE;
  optional<HlinkFeatureRequest> currently_requested_feature = {};
  std::vector<HlinkFeatureRequest> polling_features = {};
  optional<HlinkFeatureRequest> low_priority_hlink_request = {};
  std::unique_ptr<HlinkRequestFrame> currently_applying_message = nullptr;
  int16_t requested_feature_index = -1;
  uint32_t non_idle_timeout_limit_ms = 0;
  uint32_t last_status_polling_finished_at_ms = 0;
  uint32_t last_frame_received_at_ms = 0;
  uint32_t timeout_counter_started_at_ms = 0;
  uint8_t requests_left_to_apply = 0;

  void refresh_non_idle_timeout(uint32_t non_idle_timeout_limit_ms) {
    this->timeout_counter_started_at_ms = millis();
    this->non_idle_timeout_limit_ms = non_idle_timeout_limit_ms;
  }

  bool reached_timeout_thereshold() { return millis() - timeout_counter_started_at_ms > non_idle_timeout_limit_ms; }

  bool can_send_next_frame() {
    // Min interval received frame and next request frame shouldn't be less than MIN_INTERVAL_BETWEEN_REQUESTS ms or AC
    // will return NG
    return millis() - last_frame_received_at_ms > MIN_INTERVAL_BETWEEN_REQUESTS;
  }

  HlinkFeatureRequest get_currently_polling_feature() { return polling_features[requested_feature_index]; }

  void reset_state() {
    state = IDLE;
    timeout_counter_started_at_ms = 0;
    non_idle_timeout_limit_ms = 0;
    last_frame_received_at_ms = 0;
    last_status_polling_finished_at_ms = 0;
    requested_feature_index = -1;
    requests_left_to_apply = 0;
    currently_applying_message = nullptr;
    currently_requested_feature = {};
  }
};

#ifdef USE_SENSOR
enum class SensorType {
  OUTDOOR_TEMPERATURE = 0,
  // Used to count the number of sensors in the enum
  COUNT,
};
#endif
#ifdef USE_TEXT_SENSOR
enum class TextSensorType {
  MODEL_NAME,
  COUNT,
};
#endif

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

class HlinkAc : public Component, public uart::UARTDevice, public climate::Climate {
#ifdef USE_SWITCH
 public:
  void set_remote_lock_switch(switch_::Switch *sw);
  void enqueue_remote_lock_action(bool state);
  void set_beeper_switch(switch_::Switch *sw);
  void handle_beep_state_change(bool state);

 protected:
  switch_::Switch *remote_lock_switch_{nullptr};
  switch_::Switch *beeper_switch_{nullptr};
#endif
#ifdef USE_SENSOR
 public:
  void set_sensor(SensorType type, sensor::Sensor *s);

 protected:
  void update_sensor_state_(sensor::Sensor *sensor, float value);
#endif
#ifdef USE_TEXT_SENSOR
 public:
  void set_text_sensor(TextSensorType type, text_sensor::TextSensor *sens);
  void set_debug_text_sensor(uint16_t address, text_sensor::TextSensor *sens);
  void set_debug_discovery_text_sensor(text_sensor::TextSensor *sens);

 protected:
  text_sensor::TextSensor *model_name_text_sensor_{nullptr};
#endif
 public:
  // ----- COMPONENT -----
  void setup() override;
  void loop() override;
  void dump_config() override;
  // ----- END COMPONENT -----
  // ----- CLIMATE -----
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;
  void set_supported_climate_modes(const std::set<climate::ClimateMode> &modes);
  void set_supported_swing_modes(const std::set<climate::ClimateSwingMode> &modes);
  void set_supported_fan_modes(const std::set<climate::ClimateFanMode> &modes);
  // ----- END CLIMATE -----

 protected:
  ComponentStatus status_ = ComponentStatus();
  HlinkEntityStatus hlink_entity_status_ = HlinkEntityStatus();
  climate::ClimateTraits traits_ = climate::ClimateTraits();
  CircularRequestsQueue pending_action_requests;
  void request_status_update_();
  void handle_feature_write_response_ack_(HlinkRequestFrame applied_request);
  void publish_updates_if_any_();
  HlinkResponseFrame read_hlink_frame_(uint32_t timeout_ms);
  void write_hlink_frame_(HlinkRequestFrame frame);
  std::unique_ptr<HlinkRequestFrame> createRequestFrame_(
      uint16_t primary_control, uint16_t secondary_control,
      optional<HlinkRequestFrame::AttributeFormat> secondary_control_format =
          HlinkRequestFrame::AttributeFormat::TWO_DIGITS);
};
}  // namespace hlink_ac
}  // namespace esphome