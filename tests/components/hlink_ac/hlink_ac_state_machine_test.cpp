#include "common.h"
#include "hlink_test_utils.h"

namespace esphome::hlink_ac::testing {

class HlinkAcStateMachineTest : public ::testing::Test {
 protected:
  void advance_for_next_send() { this->ac_.advance_current_time_ms_for_test(MIN_INTERVAL_BETWEEN_REQUESTS + 1); }

  void SetUp() override {
    global_preferences->reset();
    this->ac_.set_uart_parent_for_test(&this->uart_);
    this->ac_.set_current_time_ms_for_test(0);
    this->ac_.set_status_update_interval(0xFFFFFF00U);
    this->ac_.set_text_sensor(TextSensorType::MODEL_NAME, nullptr);
    this->ac_.add_on_state_callback([this](climate::Climate &) { this->publish_count_++; });
  }

  void send_poll_request_and_assert(const std::string &expected_tx_string) {
    this->advance_for_next_send();
    this->ac_.loop();
    EXPECT_EQ(this->uart_.take_tx_as_string(), expected_tx_string);
    EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);
  }

  void inject_response_and_step(const std::string &response) {
    this->uart_.inject_rx(response);
    this->ac_.loop();
  }

  void run_polling_cycle(const std::vector<std::string> &responses) {
    ASSERT_EQ(responses.size(), this->ac_.status().polling_features.size());
    const std::vector<std::string> requests = {
        "MT P=0000 C=FFFF\r",  // POWER_STATE
        "MT P=0001 C=FFFE\r",  // MODE
        "MT P=0003 C=FFFC\r",  // TARGET_TEMP
        "MT P=0100 C=FFFE\r",  // CURRENT_INDOOR_TEMP
        "MT P=0002 C=FFFD\r",  // FAN_MODE
        "MT P=0900 C=FFF6\r",  // MODEL_NAME
        "MT P=0304 C=FFF8\r",  // LEAVE_HOME_STATUS_READ
    };
    for (size_t i = 0; i < responses.size(); i++) {
      this->send_poll_request_and_assert(requests[i]);
      this->inject_response_and_step(responses[i]);
    }
    EXPECT_EQ(this->ac_.state(), PUBLISH_UPDATE_IF_ANY);
    this->ac_.loop();  // CAPTURE_TARGET_TEMPERATURE
    this->ac_.loop();  // IDLE
    EXPECT_EQ(this->ac_.state(), IDLE);
  }

  // Sends the 4 minimal boot cycle requests and feeds them the given responses. The cycle completion
  // lambda has already run, so the state is PUBLISH_UPDATE_IF_ANY on success or INIT on incomplete status.
  void run_boot_cycle(const std::vector<std::string> &responses) {
    ASSERT_EQ(responses.size(), 4U);
    const std::vector<std::string> requests = {
        "MT P=0000 C=FFFF\r",  // POWER_STATE
        "MT P=0001 C=FFFE\r",  // MODE
        "MT P=0003 C=FFFC\r",  // TARGET_TEMP
        "MT P=0100 C=FFFE\r",  // CURRENT_INDOOR_TEMP
    };
    this->advance_for_next_send();
    this->ac_.loop();  // INIT: start the boot cycle
    this->ac_.loop();  // REQUEST_NEXT_STATUS_FEATURE: send the first request
    EXPECT_EQ(this->uart_.take_tx_as_string(), requests[0]);
    EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);
    this->inject_response_and_step(responses[0]);
    for (size_t i = 1; i < responses.size(); i++) {
      this->send_poll_request_and_assert(requests[i]);
      this->inject_response_and_step(responses[i]);
    }
  }

  // Runs the boot cycle tail: PUBLISH_UPDATE_IF_ANY -> CAPTURE_TARGET_TEMPERATURE -> IDLE.
  // Must be called after a successful run_boot_cycle.
  void finish_boot_cycle_to_idle() {
    EXPECT_EQ(this->ac_.state(), PUBLISH_UPDATE_IF_ANY);
    this->ac_.loop();  // PUBLISH_UPDATE_IF_ANY
    this->ac_.loop();  // CAPTURE_TARGET_TEMPERATURE
    EXPECT_EQ(this->ac_.state(), IDLE);
  }

  const std::vector<std::string> boot_cool_cycle_responses_ = {
      "OK P=01 C=FFFE\r",    // POWER_STATE: on
      "OK P=0040 C=FFBF\r",  // MODE: cool
      "OK P=0016 C=FFE9\r",  // TARGET_TEMP: 22°C
      "OK P=0018 C=FFE7\r",  // CURRENT_INDOOR_TEMP: 24°C
  };

  const std::vector<std::string> boot_off_cycle_responses_ = {
      "OK P=00 C=FFFF\r",    // POWER_STATE: off
      "OK P=0000 C=FFFF\r",  // MODE: off
      "OK P=0016 C=FFE9\r",  // TARGET_TEMP: 22°C (ignored when off)
      "OK P=0018 C=FFE7\r",  // CURRENT_INDOOR_TEMP: 24°C
  };

  const std::vector<std::string> cool_cycle_responses_ = {
      "OK P=01 C=FFFE\r",                  // POWER_STATE: on
      "OK P=0040 C=FFBF\r",                // MODE: cool
      "OK P=0016 C=FFE9\r",                // TARGET_TEMP: 22°C
      "OK P=0018 C=FFE7\r",                // CURRENT_INDOOR_TEMP: 24°C
      "OK P=01 C=FFFE\r",                  // FAN_MODE: high
      "OK P=52414B2D3235504543 C=FDB5\r",  // MODEL_NAME: "RAK-25PEC"
  };

  MockUARTComponent uart_;
  TestHlinkAc ac_;
  int publish_count_{0};
};

TEST_F(HlinkAcStateMachineTest, PollingCycleHappyPath) {
  this->ac_.request_status_update_for_test();
  ASSERT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->send_poll_request_and_assert("MT P=0000 C=FFFF\r");  // POWER_STATE
  this->inject_response_and_step("OK P=01 C=FFFE\r");        // POWER_STATE: on
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->send_poll_request_and_assert("MT P=0001 C=FFFE\r");  // MODE
  this->inject_response_and_step("OK P=0040 C=FFBF\r");      // MODE: cool
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->send_poll_request_and_assert("MT P=0003 C=FFFC\r");  // TARGET_TEMP
  this->inject_response_and_step("OK P=0016 C=FFE9\r");      // TARGET_TEMP: 22°C
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->send_poll_request_and_assert("MT P=0100 C=FFFE\r");  // CURRENT_INDOOR_TEMP
  this->inject_response_and_step("OK P=0018 C=FFE7\r");      // CURRENT_INDOOR_TEMP: 24°C
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->send_poll_request_and_assert("MT P=0002 C=FFFD\r");  // FAN_MODE
  this->inject_response_and_step("OK P=01 C=FFFE\r");        // FAN_MODE: high
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->send_poll_request_and_assert("MT P=0900 C=FFF6\r");            // MODEL_NAME
  this->inject_response_and_step("OK P=52414B2D3235504543 C=FDB5\r");  // MODEL_NAME: "RAK-25PEC"
  EXPECT_EQ(this->ac_.state(), PUBLISH_UPDATE_IF_ANY);

  this->ac_.loop();
  EXPECT_EQ(this->ac_.state(), CAPTURE_TARGET_TEMPERATURE);
  this->ac_.loop();
  EXPECT_EQ(this->ac_.state(), IDLE);
  EXPECT_EQ(this->ac_.mode, climate::ClimateMode::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(this->ac_.target_temperature, 22.0f);
  EXPECT_FLOAT_EQ(this->ac_.current_temperature, 24.0f);
  ASSERT_TRUE(this->ac_.fan_mode.has_value());
  EXPECT_EQ(this->ac_.fan_mode.value(), climate::ClimateFanMode::CLIMATE_FAN_HIGH);
  EXPECT_EQ(this->ac_.hlink_entity_status_.model_name, "RAK-25PEC");
  EXPECT_EQ(this->publish_count_, 1);
}

TEST_F(HlinkAcStateMachineTest, PollingCycleWithFailedFeatureDoesNotPublishOrCapture) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.request_status_update_for_test();
  ASSERT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->send_poll_request_and_assert("MT P=0000 C=FFFF\r");  // POWER_STATE
  this->inject_response_and_step("OK P=01 C=FFFE\r");        // POWER_STATE: on
  this->send_poll_request_and_assert("MT P=0001 C=FFFE\r");  // MODE
  this->inject_response_and_step("NG P=FFFF C=FFFF\r");      // MODE: NG
  this->send_poll_request_and_assert("MT P=0003 C=FFFC\r");  // TARGET_TEMP
  this->inject_response_and_step("OK P=0016 C=FFE9\r");      // TARGET_TEMP: 22°C
  this->send_poll_request_and_assert("MT P=0100 C=FFFE\r");  // CURRENT_INDOOR_TEMP
  this->inject_response_and_step("OK P=0018 C=FFE7\r");      // CURRENT_INDOOR_TEMP: 24°C
  this->send_poll_request_and_assert("MT P=0002 C=FFFD\r");  // FAN_MODE
  this->inject_response_and_step("OK P=01 C=FFFE\r");        // FAN_MODE: high
  this->send_poll_request_and_assert("MT P=0900 C=FFF6\r");            // MODEL_NAME
  this->inject_response_and_step("OK P=52414B2D3235504543 C=FDB5\r");  // MODEL_NAME: "RAK-25PEC"

  EXPECT_EQ(this->ac_.state(), IDLE);
  EXPECT_EQ(this->publish_count_, 0);
  auto stored_temps = this->ac_.stored_target_temperatures_for_test();
  EXPECT_FALSE(stored_temps.cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.dry_target_temperature.has_value());
}

TEST_F(HlinkAcStateMachineTest, PollingCycleWithInvalidResponseDoesNotPublishOrCapture) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.request_status_update_for_test();
  ASSERT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->send_poll_request_and_assert("MT P=0000 C=FFFF\r");  // POWER_STATE
  this->inject_response_and_step("XX P=0000 C=FFFF\r");      // POWER_STATE: INVALID
  this->send_poll_request_and_assert("MT P=0001 C=FFFE\r");  // MODE
  this->inject_response_and_step("OK P=0040 C=FFBF\r");      // MODE: cool
  this->send_poll_request_and_assert("MT P=0003 C=FFFC\r");  // TARGET_TEMP
  this->inject_response_and_step("OK P=0016 C=FFE9\r");      // TARGET_TEMP: 22°C
  this->send_poll_request_and_assert("MT P=0100 C=FFFE\r");  // CURRENT_INDOOR_TEMP
  this->inject_response_and_step("OK P=0018 C=FFE7\r");      // CURRENT_INDOOR_TEMP: 24°C
  this->send_poll_request_and_assert("MT P=0002 C=FFFD\r");  // FAN_MODE
  this->inject_response_and_step("OK P=01 C=FFFE\r");        // FAN_MODE: high
  this->send_poll_request_and_assert("MT P=0900 C=FFF6\r");            // MODEL_NAME
  this->inject_response_and_step("OK P=52414B2D3235504543 C=FDB5\r");  // MODEL_NAME: "RAK-25PEC"

  EXPECT_EQ(this->ac_.state(), IDLE);
  EXPECT_EQ(this->publish_count_, 0);
  auto stored_temps = this->ac_.stored_target_temperatures_for_test();
  EXPECT_FALSE(stored_temps.cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.dry_target_temperature.has_value());
}

TEST_F(HlinkAcStateMachineTest, PartialResponseIsCompletedOnNextLoop) {
  this->ac_.request_status_update_for_test();
  this->send_poll_request_and_assert("MT P=0000 C=FFFF\r");  // POWER_STATE

  this->inject_response_and_step("OK P=01 C=FF");  // partial: POWER_STATE
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);

  this->inject_response_and_step("FE\r");  // partial remainder (checksum)
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);
  EXPECT_EQ(this->publish_count_, 0);
}

TEST_F(HlinkAcStateMachineTest, SendsNextRequestOnlyAfterStrictlyMoreThanMinInterval) {
  this->ac_.request_status_update_for_test();
  this->send_poll_request_and_assert("MT P=0000 C=FFFF\r");  // POWER_STATE

  this->inject_response_and_step("OK P=01 C=FFFE\r");  // POWER_STATE: on
  ASSERT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->ac_.loop();
  EXPECT_TRUE(this->uart_.take_tx_as_string().empty());
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->ac_.advance_current_time_ms_for_test(MIN_INTERVAL_BETWEEN_REQUESTS);
  this->ac_.loop();
  EXPECT_TRUE(this->uart_.take_tx_as_string().empty());
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->ac_.advance_current_time_ms_for_test(1);
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(),
            "MT P=0001 C=FFFE\r");  // request MODE
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);
  EXPECT_EQ(this->publish_count_, 0);
}

TEST_F(HlinkAcStateMachineTest, AppliesQueuedRequestsAndStartsStatusRefresh) {
  int ok_callbacks_called = 0;
  this->ac_.enqueue_request_for_test(
      HlinkRequestFrame::with_uint8(HlinkRequestFrame::Type::ST, FeatureType::POWER_STATE, 0x01),
      [&](const HlinkResponseFrame &) { ok_callbacks_called++; });
  this->ac_.enqueue_request_for_test(
      HlinkRequestFrame::with_uint16(HlinkRequestFrame::Type::ST, FeatureType::MODE, HLINK_MODE_COOL),
      [&](const HlinkResponseFrame &) { ok_callbacks_called++; });

  this->ac_.loop();
  EXPECT_EQ(this->ac_.state(), APPLY_REQUEST);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(),
            "ST P=0000,01 C=FFFE\r");  // set POWER_STATE: on
  EXPECT_EQ(this->ac_.state(), ACK_APPLIED_REQUEST);

  this->inject_response_and_step(ACK_OK_FRAME);
  EXPECT_EQ(this->ac_.state(), APPLY_REQUEST);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0001,0040 C=FFBE\r");  // set MODE: cool
  EXPECT_EQ(this->ac_.state(), ACK_APPLIED_REQUEST);

  this->inject_response_and_step(ACK_OK_FRAME);
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);
  EXPECT_EQ(ok_callbacks_called, 2);
  EXPECT_EQ(this->publish_count_, 0);
}

TEST_F(HlinkAcStateMachineTest, HandlesLowPriorityRequestFromIdle) {
  bool callback_called = false;
  std::string payload_string;
  this->ac_.set_low_priority_request_for_test({HlinkRequestFrame::Type::MT, {FeatureType::MODEL_NAME}},
                                              [&](const HlinkResponseFrame &response) {
                                                callback_called = true;
                                                payload_string = response.p_value_as_string().value_or("");
                                              });

  this->ac_.loop();
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(),
            "MT P=0900 C=FFF6\r");  // request MODEL_NAME
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);

  this->inject_response_and_step("OK P=52414B2D3235504543 C=FDB5\r");  // MODEL_NAME: "RAK-25PEC"
  EXPECT_EQ(this->ac_.state(), IDLE);
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(payload_string, "52414B2D3235504543");
  EXPECT_EQ(this->publish_count_, 0);
}

TEST_F(HlinkAcStateMachineTest, LowPriorityCycleDoesNotDelayNextStatusPolling) {
  this->ac_.set_status_update_interval(1000);
  this->ac_.set_low_priority_request_for_test({HlinkRequestFrame::Type::MT, {FeatureType::MODEL_NAME}});

  this->ac_.loop();
  ASSERT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);
  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "MT P=0900 C=FFF6\r");
  this->inject_response_and_step("OK P=52414B2D3235504543 C=FDB5\r");
  EXPECT_EQ(this->ac_.state(), IDLE);
  // A low-priority (discovery) cycle must not re-arm the status polling interval.
  EXPECT_EQ(this->ac_.status().last_status_polling_finished_at_ms, 0);

  // Status polling still starts as soon as the update interval has elapsed.
  this->ac_.advance_current_time_ms_for_test(1000);
  this->ac_.loop();
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);
}

TEST_F(HlinkAcStateMachineTest, InvokesTimeoutCallbackAndResetsState) {
  bool timeout_called = false;
  this->ac_.set_low_priority_request_for_test({HlinkRequestFrame::Type::MT, {FeatureType::MODEL_NAME}}, nullptr,
                                              nullptr, nullptr, [&]() { timeout_called = true; });

  this->ac_.loop();
  ASSERT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);
  EXPECT_EQ(this->uart_.take_tx_as_string(),
            "MT P=0900 C=FFF6\r");  // request MODEL_NAME

  this->ac_.advance_current_time_ms_for_test(501);
  this->ac_.loop();

  EXPECT_TRUE(timeout_called);
  EXPECT_EQ(this->ac_.state(), IDLE);
  EXPECT_EQ(this->ac_.status().current_request, nullptr);
  EXPECT_EQ(this->publish_count_, 0);
}

TEST_F(HlinkAcStateMachineTest, BootPollingCycleSendsOnlyMinimalRequestsAndTransitionsToIdle) {
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);

  this->run_boot_cycle(this->boot_cool_cycle_responses_);
  EXPECT_EQ(this->ac_.state(), PUBLISH_UPDATE_IF_ANY);
  this->finish_boot_cycle_to_idle();
  EXPECT_TRUE(this->uart_.take_tx_as_string().empty());
  EXPECT_EQ(this->publish_count_, 1);
}

TEST_F(HlinkAcStateMachineTest, BootPollingRetriesToInitOnIncompleteStatus) {
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);

  this->run_boot_cycle({"OK P=01 C=FFFE\r", "NG P=FFFF C=FFFF\r", "OK P=0016 C=FFE9\r",
                        "OK P=0018 C=FFE7\r"});  // MODE: NG, so the minimal status is incomplete
  EXPECT_EQ(this->ac_.state(), INIT);
  EXPECT_EQ(this->publish_count_, 0);

  this->advance_for_next_send();
  this->ac_.loop();  // INIT: start the boot cycle
  this->ac_.loop();  // REQUEST_NEXT_STATUS_FEATURE: send the retry request
  EXPECT_EQ(this->uart_.take_tx_as_string(), "MT P=0000 C=FFFF\r");  // boot cycle retry
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);
}

TEST_F(HlinkAcStateMachineTest, ControlDoesNotCaptureTargetTemperature) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.set_reference_temperature(23);
  this->ac_.setup();

  auto call = this->ac_.make_call();
  call.set_mode(climate::ClimateMode::CLIMATE_MODE_COOL).set_target_temperature(24.0f);
  this->ac_.control(call);

  auto stored_temps = this->ac_.stored_target_temperatures_for_test();
  EXPECT_FALSE(stored_temps.cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.dry_target_temperature.has_value());
}

TEST_F(HlinkAcStateMachineTest, PollingCapturesTargetTemperaturePerMode) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.request_status_update_for_test();
  this->run_polling_cycle(this->cool_cycle_responses_);

  auto stored_temps = this->ac_.stored_target_temperatures_for_test();
  ASSERT_TRUE(stored_temps.cool_target_temperature.has_value());
  EXPECT_FLOAT_EQ(stored_temps.cool_target_temperature.value(), 22.0f);
  EXPECT_FALSE(stored_temps.heat_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.dry_target_temperature.has_value());
}

TEST_F(HlinkAcStateMachineTest, PollingCapturesAutoModeTargetTemperature) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.set_reference_temperature(23);
  this->ac_.request_status_update_for_test();
  this->run_polling_cycle({"OK P=01 C=FFFE\r",                    // POWER_STATE: on
                           "OK P=8010 C=FF6F\r",                  // MODE: heat auto
                           "OK P=FFFF C=FE01\r",                  // TARGET_TEMP: offset -1
                           "OK P=0018 C=FFE7\r",                  // CURRENT_INDOOR_TEMP: 24°C
                           "OK P=01 C=FFFE\r",                    // FAN_MODE: high
                           "OK P=52414B2D3235504543 C=FDB5\r"});  // MODEL_NAME: "RAK-25PEC"

  auto stored_temps = this->ac_.stored_target_temperatures_for_test();
  ASSERT_TRUE(stored_temps.heat_cool_target_temperature.has_value());
  // Auto heating offset -1 is adjusted to -3, clamped to the auto range [20; 26].
  EXPECT_FLOAT_EQ(stored_temps.heat_cool_target_temperature.value(), 20.0f);
  EXPECT_FALSE(stored_temps.cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.dry_target_temperature.has_value());
}

TEST_F(HlinkAcStateMachineTest, PollingDoesNotCaptureWhenAcOff) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.request_status_update_for_test();
  this->run_polling_cycle({"OK P=00 C=FFFF\r",                    // POWER_STATE: off
                           "OK P=0000 C=FFFF\r",                  // MODE: off
                           "OK P=0016 C=FFE9\r",                  // TARGET_TEMP: 22°C (ignored when off)
                           "OK P=0018 C=FFE7\r",                  // CURRENT_INDOOR_TEMP: 24°C
                           "OK P=01 C=FFFE\r",                    // FAN_MODE: high
                           "OK P=52414B2D3235504543 C=FDB5\r"});  // MODEL_NAME: "RAK-25PEC"

  auto stored_temps = this->ac_.stored_target_temperatures_for_test();
  EXPECT_FALSE(stored_temps.cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.dry_target_temperature.has_value());
}

TEST_F(HlinkAcStateMachineTest, PollingDoesNotCaptureWhenRememberDisabled) {
  this->ac_.request_status_update_for_test();
  this->run_polling_cycle(this->cool_cycle_responses_);

  auto stored_temps = this->ac_.stored_target_temperatures_for_test();
  EXPECT_FALSE(stored_temps.cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.dry_target_temperature.has_value());
}

TEST_F(HlinkAcStateMachineTest, PollingDoesNotCaptureAwayModeTargetTemperature) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.set_supported_climate_presets({climate::ClimatePreset::CLIMATE_PRESET_AWAY});
  this->ac_.request_status_update_for_test();
  this->run_polling_cycle({"OK P=01 C=FFFE\r",                  // POWER_STATE: on
                           "OK P=0010 C=FFEF\r",                // MODE: heat
                           "OK P=000A C=FFF5\r",                // TARGET_TEMP: 10°C (away marker)
                           "OK P=0018 C=FFE7\r",                // CURRENT_INDOOR_TEMP: 24°C
                           "OK P=01 C=FFFE\r",                  // FAN_MODE: high
                           "OK P=52414B2D3235504543 C=FDB5\r",  // MODEL_NAME: "RAK-25PEC"
                           "OK P=00000080 C=FF7F\r"});          // LEAVE_HOME_STATUS_READ: enabled

  auto stored_temps = this->ac_.stored_target_temperatures_for_test();
  EXPECT_FALSE(stored_temps.cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.dry_target_temperature.has_value());
}

TEST_F(HlinkAcStateMachineTest, PollingDoesNotCaptureAwayModeTargetTemperatureWhenLeaveHomeStatusUnknown) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.request_status_update_for_test();
  this->run_polling_cycle({"OK P=01 C=FFFE\r",                    // POWER_STATE: on
                           "OK P=0010 C=FFEF\r",                  // MODE: heat
                           "OK P=000A C=FFF5\r",                  // TARGET_TEMP: 10°C (away marker)
                           "OK P=0018 C=FFE7\r",                  // CURRENT_INDOOR_TEMP: 24°C
                           "OK P=01 C=FFFE\r",                    // FAN_MODE: high
                           "OK P=52414B2D3235504543 C=FDB5\r"});  // MODEL_NAME: "RAK-25PEC"

  auto stored_temps = this->ac_.stored_target_temperatures_for_test();
  EXPECT_FALSE(stored_temps.cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.dry_target_temperature.has_value());
}

TEST_F(HlinkAcStateMachineTest, PollingSavesTargetTemperaturesOnlyOnChange) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.request_status_update_for_test();
  this->run_polling_cycle(this->cool_cycle_responses_);
  EXPECT_EQ(this->ac_.save_settings_call_count_for_test(), 1);

  this->ac_.request_status_update_for_test();
  this->run_polling_cycle(this->cool_cycle_responses_);
  EXPECT_EQ(this->ac_.save_settings_call_count_for_test(), 1);

  this->ac_.request_status_update_for_test();
  this->run_polling_cycle({"OK P=01 C=FFFE\r",                    // POWER_STATE: on
                           "OK P=0040 C=FFBF\r",                  // MODE: cool
                           "OK P=0017 C=FFE8\r",                  // TARGET_TEMP: 23°C
                           "OK P=0018 C=FFE7\r",                  // CURRENT_INDOOR_TEMP: 24°C
                           "OK P=01 C=FFFE\r",                    // FAN_MODE: high
                           "OK P=52414B2D3235504543 C=FDB5\r"});  // MODEL_NAME: "RAK-25PEC"
  EXPECT_EQ(this->ac_.save_settings_call_count_for_test(), 2);

  auto stored_temps = this->ac_.stored_target_temperatures_for_test();
  ASSERT_TRUE(stored_temps.cool_target_temperature.has_value());
  EXPECT_FLOAT_EQ(stored_temps.cool_target_temperature.value(), 23.0f);
}

TEST_F(HlinkAcStateMachineTest, PersistsTargetTemperaturesAcrossRestarts) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);
  this->run_boot_cycle({"OK P=01 C=FFFE\r",      // POWER_STATE: on
                        "OK P=0010 C=FFEF\r",    // MODE: heat
                        "OK P=001A C=FFE5\r",    // TARGET_TEMP: 26°C
                        "OK P=0018 C=FFE7\r"});  // CURRENT_INDOOR_TEMP: 24°C
  this->finish_boot_cycle_to_idle();
  EXPECT_EQ(this->publish_count_, 1);

  this->ac_.request_status_update_for_test();
  this->run_polling_cycle({"OK P=01 C=FFFE\r",                    // POWER_STATE: on
                           "OK P=0010 C=FFEF\r",                  // MODE: heat
                           "OK P=001A C=FFE5\r",                  // TARGET_TEMP: 26°C
                           "OK P=0018 C=FFE7\r",                  // CURRENT_INDOOR_TEMP: 24°C
                           "OK P=01 C=FFFE\r",                    // FAN_MODE: high
                           "OK P=52414B2D3235504543 C=FDB5\r"});  // MODEL_NAME: "RAK-25PEC"

  TestHlinkAc restarted_ac;
  restarted_ac.set_uart_parent_for_test(&this->uart_);
  restarted_ac.set_current_time_ms_for_test(0);
  restarted_ac.setup();

  auto stored_temps = restarted_ac.stored_target_temperatures_for_test();
  ASSERT_TRUE(stored_temps.heat_target_temperature.has_value());
  EXPECT_FLOAT_EQ(stored_temps.heat_target_temperature.value(), 26.0f);
  EXPECT_FALSE(stored_temps.cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.dry_target_temperature.has_value());
}

TEST_F(HlinkAcStateMachineTest, RestoresHeatTargetTemperatureOutsideAutoRange) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);
  this->run_boot_cycle({"OK P=01 C=FFFE\r",      // POWER_STATE: on
                        "OK P=0010 C=FFEF\r",    // MODE: heat
                        "OK P=001E C=FFE1\r",    // TARGET_TEMP: 30°C
                        "OK P=0018 C=FFE7\r"});  // CURRENT_INDOOR_TEMP: 24°C
  this->finish_boot_cycle_to_idle();
  EXPECT_EQ(this->publish_count_, 1);

  this->ac_.request_status_update_for_test();
  this->run_polling_cycle({"OK P=01 C=FFFE\r",                    // POWER_STATE: on
                           "OK P=0010 C=FFEF\r",                  // MODE: heat
                           "OK P=001E C=FFE1\r",                  // TARGET_TEMP: 30°C
                           "OK P=0018 C=FFE7\r",                  // CURRENT_INDOOR_TEMP: 24°C
                           "OK P=01 C=FFFE\r",                    // FAN_MODE: high
                           "OK P=52414B2D3235504543 C=FDB5\r"});  // MODEL_NAME: "RAK-25PEC"

  TestHlinkAc restarted_ac;
  restarted_ac.set_uart_parent_for_test(&this->uart_);
  restarted_ac.set_current_time_ms_for_test(0);
  restarted_ac.setup();

  auto stored_temps = restarted_ac.stored_target_temperatures_for_test();
  ASSERT_TRUE(stored_temps.heat_target_temperature.has_value());
  EXPECT_FLOAT_EQ(stored_temps.heat_target_temperature.value(), 30.0f);
}

TEST_F(HlinkAcStateMachineTest, DoesNotRestoreWhenAcIsOnAtFirstPoll) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);

  StoredTargetTemperatures stored_temps{};
  stored_temps.cool_target_temperature = 24.0f;
  this->ac_.set_stored_target_temperatures_for_test(stored_temps);

  this->run_boot_cycle(this->boot_cool_cycle_responses_);
  this->finish_boot_cycle_to_idle();
  EXPECT_TRUE(this->uart_.take_tx_as_string().empty());  // no restore ST requests when the AC is on
  EXPECT_EQ(this->publish_count_, 1);

  // The polled target temperature was captured instead of restoring the stored one.
  auto current_temps = this->ac_.stored_target_temperatures_for_test();
  ASSERT_TRUE(current_temps.cool_target_temperature.has_value());
  EXPECT_FLOAT_EQ(current_temps.cool_target_temperature.value(), 22.0f);
}

TEST_F(HlinkAcStateMachineTest, ControlRestoresStoredTargetTemperatureWhenTurningOn) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);

  StoredTargetTemperatures stored_temps{};
  stored_temps.cool_target_temperature = 24.0f;
  this->ac_.set_stored_target_temperatures_for_test(stored_temps);

  this->run_boot_cycle(this->boot_off_cycle_responses_);
  this->finish_boot_cycle_to_idle();
  EXPECT_TRUE(this->uart_.take_tx_as_string().empty());

  // The AC is turned on from Home Assistant without a target temperature, e.g. after a power cut.
  this->ac_.control(this->ac_.make_call().set_mode(climate::ClimateMode::CLIMATE_MODE_COOL));
  this->ac_.loop();  // pending requests -> APPLY_REQUEST
  EXPECT_EQ(this->ac_.state(), APPLY_REQUEST);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0000,01 C=FFFE\r");  // set POWER_STATE: on
  this->inject_response_and_step(ACK_OK_FRAME);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0001,0040 C=FFBE\r");  // set MODE: cool
  this->inject_response_and_step(ACK_OK_FRAME);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0003,0018 C=FFE4\r");  // TARGET_TEMP: 24°C (restored)
  EXPECT_EQ(this->ac_.state(), ACK_APPLIED_REQUEST);

  this->inject_response_and_step(ACK_OK_FRAME);
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);  // status refresh after the restore apply
}

TEST_F(HlinkAcStateMachineTest, ControlDoesNotRestoreWhenCallIncludesTargetTemperature) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);

  StoredTargetTemperatures stored_temps{};
  stored_temps.cool_target_temperature = 24.0f;
  this->ac_.set_stored_target_temperatures_for_test(stored_temps);

  this->run_boot_cycle(this->boot_off_cycle_responses_);
  this->finish_boot_cycle_to_idle();
  EXPECT_TRUE(this->uart_.take_tx_as_string().empty());

  // The user turns the AC on and sets 22°C in the same call; the explicit value must win over the stored 24°C.
  auto call = this->ac_.make_call();
  call.set_mode(climate::ClimateMode::CLIMATE_MODE_COOL).set_target_temperature(22.0f);
  this->ac_.control(call);
  this->ac_.loop();  // pending requests -> APPLY_REQUEST
  EXPECT_EQ(this->ac_.state(), APPLY_REQUEST);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0000,01 C=FFFE\r");  // set POWER_STATE: on
  this->inject_response_and_step(ACK_OK_FRAME);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0001,0040 C=FFBE\r");  // set MODE: cool
  this->inject_response_and_step(ACK_OK_FRAME);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0003,0016 C=FFE6\r");  // TARGET_TEMP: 22°C
  this->inject_response_and_step(ACK_OK_FRAME);

  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);  // no restore request follows the explicit write
}

TEST_F(HlinkAcStateMachineTest, ControlRestoresRememberedTemperatureWhenSwitchingModes) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);

  StoredTargetTemperatures stored_temps{};
  stored_temps.heat_target_temperature = 24.0f;
  this->ac_.set_stored_target_temperatures_for_test(stored_temps);

  this->run_boot_cycle(this->boot_cool_cycle_responses_);
  this->finish_boot_cycle_to_idle();
  EXPECT_TRUE(this->uart_.take_tx_as_string().empty());

  // Switching from cool to heat must restore the remembered heat target temperature.
  this->ac_.control(this->ac_.make_call().set_mode(climate::ClimateMode::CLIMATE_MODE_HEAT));
  this->ac_.loop();  // pending requests -> APPLY_REQUEST
  EXPECT_EQ(this->ac_.state(), APPLY_REQUEST);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0000,01 C=FFFE\r");  // set POWER_STATE: on
  this->inject_response_and_step(ACK_OK_FRAME);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0001,0010 C=FFEE\r");  // set MODE: heat
  this->inject_response_and_step(ACK_OK_FRAME);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0003,0018 C=FFE4\r");  // TARGET_TEMP: 24°C (restored)
  EXPECT_EQ(this->ac_.state(), ACK_APPLIED_REQUEST);

  this->inject_response_and_step(ACK_OK_FRAME);
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);  // status refresh after the restore apply
}

TEST_F(HlinkAcStateMachineTest, ControlDoesNotRestoreWhenAcAlreadyInRequestedMode) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);

  StoredTargetTemperatures stored_temps{};
  stored_temps.cool_target_temperature = 24.0f;
  this->ac_.set_stored_target_temperatures_for_test(stored_temps);

  this->run_boot_cycle(this->boot_cool_cycle_responses_);
  this->finish_boot_cycle_to_idle();
  EXPECT_TRUE(this->uart_.take_tx_as_string().empty());

  // Re-selecting the mode the AC is already running in must not clobber its current temperature.
  this->ac_.control(this->ac_.make_call().set_mode(climate::ClimateMode::CLIMATE_MODE_COOL));
  this->ac_.loop();  // pending requests -> APPLY_REQUEST
  EXPECT_EQ(this->ac_.state(), APPLY_REQUEST);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0000,01 C=FFFE\r");  // set POWER_STATE: on
  this->inject_response_and_step(ACK_OK_FRAME);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0001,0040 C=FFBE\r");  // set MODE: cool
  this->inject_response_and_step(ACK_OK_FRAME);

  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);  // no restore request follows the mode write
}

TEST_F(HlinkAcStateMachineTest, ControlDoesNotRestoreOnAwayPresetTurnOn) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);

  StoredTargetTemperatures stored_temps{};
  stored_temps.heat_target_temperature = 24.0f;
  this->ac_.set_stored_target_temperatures_for_test(stored_temps);

  this->run_boot_cycle(this->boot_off_cycle_responses_);
  this->finish_boot_cycle_to_idle();
  EXPECT_TRUE(this->uart_.take_tx_as_string().empty());

  // Entering away mode turns the AC on via the leave home sequence; it must not restore a regular temperature.
  auto call = this->ac_.make_call();
  call.set_mode(climate::ClimateMode::CLIMATE_MODE_HEAT).set_preset(climate::ClimatePreset::CLIMATE_PRESET_AWAY);
  this->ac_.control(call);
  this->ac_.loop();  // pending requests -> APPLY_REQUEST
  EXPECT_EQ(this->ac_.state(), APPLY_REQUEST);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0000,01 C=FFFE\r");  // set POWER_STATE: on
  this->inject_response_and_step(ACK_OK_FRAME);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0001,0010 C=FFEE\r");  // set MODE: heat
  this->inject_response_and_step(ACK_OK_FRAME);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0001,0010 C=FFEE\r");  // set MODE: heat (leave home sequence)
  this->inject_response_and_step(ACK_OK_FRAME);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0300,0040 C=FFBC\r");  // set LEAVE_HOME_STATUS_WRITE: enable
  this->inject_response_and_step(ACK_OK_FRAME);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0000,01 C=FFFE\r");  // set POWER_STATE: on (leave home sequence)
  this->inject_response_and_step(ACK_OK_FRAME);

  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);  // no restore request in the leave home sequence
}

TEST_F(HlinkAcStateMachineTest, ControlRestoresHeatCoolModeAutoEncodedTargetTemperature) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.set_reference_temperature(25);
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);

  StoredTargetTemperatures stored_temps{};
  stored_temps.heat_cool_target_temperature = 24.0f;  // offset -1 from the reference temperature
  this->ac_.set_stored_target_temperatures_for_test(stored_temps);

  this->run_boot_cycle(this->boot_off_cycle_responses_);
  this->finish_boot_cycle_to_idle();
  EXPECT_TRUE(this->uart_.take_tx_as_string().empty());

  // The AC is turned on from Home Assistant in HEAT_COOL (auto) mode without a target temperature.
  this->ac_.control(this->ac_.make_call().set_mode(climate::ClimateMode::CLIMATE_MODE_HEAT_COOL));
  this->ac_.loop();  // pending requests -> APPLY_REQUEST
  EXPECT_EQ(this->ac_.state(), APPLY_REQUEST);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0000,01 C=FFFE\r");  // set POWER_STATE: on
  this->inject_response_and_step(ACK_OK_FRAME);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0001,8000 C=FF7E\r");  // set MODE: auto (HEAT_COOL)
  this->inject_response_and_step(ACK_OK_FRAME);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0003,FFFF C=FDFE\r");  // TARGET_TEMP: offset -1 (24°C)
  EXPECT_EQ(this->ac_.state(), ACK_APPLIED_REQUEST);

  this->inject_response_and_step(ACK_OK_FRAME);
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);  // status refresh after the restore apply
}

TEST_F(HlinkAcStateMachineTest, BootPollingTimedOutCycleRetriesToInit) {
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);

  this->advance_for_next_send();
  this->ac_.loop();  // INIT: start the boot cycle
  this->ac_.loop();  // REQUEST_NEXT_STATUS_FEATURE: send the first request
  EXPECT_EQ(this->uart_.take_tx_as_string(), "MT P=0000 C=FFFF\r");
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);

  this->ac_.advance_current_time_ms_for_test(2001);
  this->ac_.loop();

  EXPECT_EQ(this->ac_.state(), INIT);  // boot cycle retries without backoff
  EXPECT_EQ(this->ac_.status().current_request, nullptr);
  EXPECT_EQ(this->publish_count_, 0);

  this->advance_for_next_send();
  this->ac_.loop();  // INIT: start the boot cycle
  this->ac_.loop();  // REQUEST_NEXT_STATUS_FEATURE: send the retry request
  EXPECT_EQ(this->uart_.take_tx_as_string(), "MT P=0000 C=FFFF\r");
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);
}

}  // namespace esphome::hlink_ac::testing
