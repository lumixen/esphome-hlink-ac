#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/climate/climate.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

namespace esphome {
namespace hlink_ac {

constexpr uint8_t HLINK_MSG_READ_BUFFER_SIZE = 64;
constexpr uint8_t ASCII_CR = 0x0D;

static const std::string HLINK_MSG_OK_TOKEN = "OK";
static const std::string HLINK_MSG_NG_TOKEN = "NG";

static const std::string TIMEOUT = "TIMEOUT";

constexpr uint8_t PROTOCOL_TARGET_TEMP_MIN = 10;
constexpr uint8_t PROTOCOL_TARGET_TEMP_MAX = 32;

constexpr uint32_t MIN_INTERVAL_BETWEEN_REQUESTS = 60;

constexpr uint32_t DEFAULT_STATUS_UPDATE_INTERVAL = 5000;

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
  optional<uint16_t> hlink_climate_mode;
  optional<esphome::climate::ClimateMode> mode;
  optional<esphome::climate::ClimateAction> action;
  optional<float> current_temperature;
  optional<float> target_temperature;
  optional<int8_t> target_temperature_auto_offset;
  optional<float> current_temperature_auto_offset;
  optional<esphome::climate::ClimateFanMode> fan_mode;
  optional<esphome::climate::ClimateSwingMode> swing_mode;
  optional<bool> leave_home_enabled;
  optional<std::string> model_name;
#ifdef USE_SWITCH
  optional<bool> remote_control_lock;
#endif
  bool has_hvac_status() {
    return power_state.has_value() && current_temperature.has_value() && target_temperature.has_value() &&
           mode.has_value();
  }

  uint16_t hlink_auto_offset_temperature() {
    return static_cast<uint8_t>(target_temperature_auto_offset.value_or(0)) + 0xFF00;
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
  LEAVE_HOME_STATUS_WRITE = 0x0300,
  ACTIVITY_STATUS = 0x0301,  // 0000=Stand-by FFFF=Active
  AIR_FILTER_WARNING = 0x302,
  LEAVE_HOME_STATUS_READ = 0x0304,
  BEEPER = 0x0800,  // Triggers beeper sound
  MODEL_NAME = 0x0900,
};

constexpr uint16_t HLINK_MODE_HEAT = 0x0010;
constexpr uint16_t HLINK_MODE_HEAT_AUTO = 0x8010;
constexpr uint16_t HLINK_MODE_COOL = 0x0040;
constexpr uint16_t HLINK_MODE_COOL_AUTO = 0x8040;
constexpr uint16_t HLINK_MODE_DRY = 0x0020;
constexpr uint16_t HLINK_MODE_DRY_AUTO = 0x8020;
constexpr uint16_t HLINK_MODE_FAN = 0x0050;
constexpr uint16_t HLINK_MODE_AUTO = 0x8000;

constexpr uint8_t HLINK_SWING_OFF = 0x00;
constexpr uint8_t HLINK_SWING_VERTICAL = 0x01;
constexpr uint8_t HLINK_SWING_HORIZONTAL = 0x02;
constexpr uint8_t HLINK_SWING_BOTH = 0x03;

constexpr uint8_t HLINK_FAN_AUTO = 0x00;
constexpr uint8_t HLINK_FAN_HIGH = 0x01;
constexpr uint8_t HLINK_FAN_MEDIUM = 0x02;
constexpr uint8_t HLINK_FAN_LOW = 0x03;
constexpr uint8_t HLINK_FAN_QUIET = 0x04;

constexpr uint16_t HLINK_REMOTE_LOCK_ON = 0x0001;
constexpr uint16_t HLINK_REMOTE_LOCK_OFF = 0x0000;

constexpr uint8_t HLINK_BEEP_ACTION = 0x07;

constexpr uint16_t HLINK_ACTIVE_ON = 0xFFFF;

const uint8_t HLINK_LEAVE_HOME_ENABLED = 0x80;
const uint8_t HLINK_LEAVE_HOME_DISABLED = 0x00;

const uint16_t HLINK_ENABLE_LEAVE_HOME = 0x0040;
const uint16_t HLINK_DISABLE_LEAVE_HOME = 0x0000;

struct HlinkRequestFrame {
  enum class Type { MT, ST };
  struct ProgramPayload {
    uint16_t address;
    optional<std::vector<uint8_t>> data;
  };
  Type type;
  ProgramPayload p;

  static HlinkRequestFrame with_uint8(HlinkRequestFrame::Type type, uint16_t address, uint8_t data) {
    return {type, {address, std::vector<uint8_t>{data}}};
  }

  static HlinkRequestFrame with_uint16(HlinkRequestFrame::Type type, uint16_t address, uint16_t data) {
    return {
        type,
        {address, std::vector<uint8_t>{static_cast<uint8_t>((data >> 8) & 0xFF), static_cast<uint8_t>(data & 0xFF)}}};
  }

  static HlinkRequestFrame with_string(HlinkRequestFrame::Type type, uint16_t address, const std::string &data) {
    std::vector<uint8_t> vector_data;
    for (size_t i = 0; i < data.length(); i += 2) {
      vector_data.push_back(static_cast<uint8_t>(std::stoi(data.substr(i, 2), nullptr, 16)));
    }
    return {type, {address, vector_data}};
  }
};
struct HlinkResponseFrame {
  enum class Status { NOTHING, PARTIAL, OK, NG, INVALID };
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

struct HlinkRequest {
  HlinkRequestFrame request_frame;
  std::function<void(const HlinkResponseFrame &response)> ok_callback;
  std::function<void()> ng_callback;
  std::function<void()> invalid_callback;
  std::function<void()> timeout_callback;
};

struct ComponentStatus {
  HlinkComponentState state = IDLE;
  std::string hlink_response_buffer = std::string(HLINK_MSG_READ_BUFFER_SIZE, '\0');
  uint8_t hlink_response_buffer_index = 0;
  std::unique_ptr<HlinkRequest> current_request = nullptr;
  std::vector<HlinkRequest> polling_features = {};
  optional<HlinkRequest> low_priority_hlink_request = {};
  int16_t requested_feature_index = -1;
  uint32_t status_update_interval_ms = DEFAULT_STATUS_UPDATE_INTERVAL;
  uint32_t non_idle_timeout_limit_ms = 0;
  uint32_t last_status_polling_finished_at_ms = 0;
  uint32_t last_frame_received_at_ms = 0;
  uint32_t timeout_counter_started_at_ms = 0;
  uint8_t requests_left_to_apply = 0;

  void refresh_non_idle_timeout(uint32_t non_idle_timeout_limit_ms) {
    this->timeout_counter_started_at_ms = millis();
    this->non_idle_timeout_limit_ms = non_idle_timeout_limit_ms;
  }

  bool reached_timeout_threshold() { return millis() - timeout_counter_started_at_ms > non_idle_timeout_limit_ms; }

  bool can_send_next_frame() {
    // Min interval between received frame and next request frame shouldn't be less than MIN_INTERVAL_BETWEEN_REQUESTS
    // ms or AC will return NG
    return millis() - last_frame_received_at_ms > MIN_INTERVAL_BETWEEN_REQUESTS;
  }

  bool can_start_next_polling() { return (last_status_polling_finished_at_ms + status_update_interval_ms) < millis(); }

  HlinkRequest get_currently_polling_feature() { return polling_features[requested_feature_index]; }

  void reset_state() {
    state = IDLE;
    timeout_counter_started_at_ms = 0;
    non_idle_timeout_limit_ms = 0;
    last_status_polling_finished_at_ms = 0;
    requested_feature_index = -1;
    requests_left_to_apply = 0;
    current_request = nullptr;
  }

  void reset_response_buffer() {
    hlink_response_buffer_index = 0;
    hlink_response_buffer.assign(HLINK_MSG_READ_BUFFER_SIZE, '\0');
  }
};

struct SendHlinkCmdResult {
  std::string result_status;
  std::string cmd_type;
  std::string request_address;
  optional<std::string> request_data;
  optional<std::string> response_data;
};

#ifdef USE_SENSOR
enum class SensorType {
  OUTDOOR_TEMPERATURE = 0,
  AUTO_TARGET_TEMP_OFFSET = 1,
  // Used to count the number of sensors in the enum
  COUNT,
};
#endif
#ifdef USE_BINARY_SENSOR
enum class BinarySensorType {
  AIR_FILTER_WARNING = 0,
  COUNT,
};
#endif
#ifdef USE_TEXT_SENSOR
enum class TextSensorType {
  MODEL_NAME,
  COUNT,
};
#endif

struct HlinkAcSettings {
  bool beeper_enabled;
  int8_t auto_temperature_offset;
};

static const uint8_t REQUESTS_QUEUE_SIZE = 16;
class CircularRequestsQueue {
 public:
  int8_t enqueue(std::unique_ptr<HlinkRequest> request);
  std::unique_ptr<HlinkRequest> dequeue();
  bool is_empty();
  bool is_full();
  uint8_t size();

 protected:
  int front_{-1};
  int rear_{-1};
  uint8_t size_{0};
  std::unique_ptr<HlinkRequest> requests_[REQUESTS_QUEUE_SIZE];
};

class HlinkAc : public Component, public uart::UARTDevice, public climate::Climate {
#ifdef USE_SWITCH
 public:
  void set_remote_lock_switch(switch_::Switch *sw);
  void set_remote_lock_state(bool state);
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
  sensor::Sensor *auto_target_temp_offset_sensor_{nullptr};
#endif
#ifdef USE_BINARY_SENSOR
 public:
  void set_binary_sensor(BinarySensorType type, binary_sensor::BinarySensor *s);
#endif
#ifdef USE_TEXT_SENSOR
 public:
  void set_text_sensor(TextSensorType type, text_sensor::TextSensor *sens);
  void set_debug_text_sensor(uint16_t address, text_sensor::TextSensor *sens);
  void set_debug_discovery_text_sensor(text_sensor::TextSensor *sens);

 protected:
  text_sensor::TextSensor *model_name_text_sensor_{nullptr};
#endif
#ifdef USE_NUMBER
  SUB_NUMBER(temperature_offset)

 public:
  void set_auto_temperature_offset(float offset);
#endif
 public:
  HlinkAc();
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
  void set_supported_climate_presets(const std::set<climate::ClimatePreset> &presets);
  void set_support_hvac_actions(bool support_hvac_actions);
  // ----- END CLIMATE -----

  void set_status_update_interval(uint32_t interval_ms);
  void send_hlink_cmd(std::string cmd_type, std::string address, optional<std::string> data);
  void add_send_hlink_cmd_result_callback(std::function<void(const SendHlinkCmdResult&)> &&callback);

 protected:
  ComponentStatus status_ = ComponentStatus();
  HlinkEntityStatus hlink_entity_status_ = HlinkEntityStatus();
  climate::ClimateTraits traits_ = climate::ClimateTraits();
  CircularRequestsQueue pending_action_requests_;
  ESPPreferenceObject rtc_;
  CallbackManager<void(const SendHlinkCmdResult&)> send_hlink_cmd_result_callback_{};
  void request_status_update_();
  bool handle_hlink_request_response_(const HlinkRequest &request, const HlinkResponseFrame &response);
  void publish_updates_if_any_();
  HlinkResponseFrame read_hlink_frame_();
  void write_hlink_frame_(HlinkRequestFrame frame);
  void enqueue_request_(HlinkRequestFrame request_frame,
                        std::function<void(const HlinkResponseFrame &response)> ok_callback = nullptr,
                        std::function<void()> ng_callback = nullptr, std::function<void()> invalid_callback = nullptr,
                        std::function<void()> timeout_callback = nullptr);
  // ----- Utils -----
  bool is_nanable_equal_(float a, float b) { return (std::isnan(a) && std::isnan(b)) || (a == b); }
  void save_settings_();
};
}  // namespace hlink_ac
}  // namespace esphome