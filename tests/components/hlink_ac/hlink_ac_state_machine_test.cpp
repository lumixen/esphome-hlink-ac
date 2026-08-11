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

  EXPECT_EQ(this->ac_.state(), IDLE);
  EXPECT_EQ(this->ac_.mode, climate::ClimateMode::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(this->ac_.target_temperature, 22.0f);
  EXPECT_FLOAT_EQ(this->ac_.current_temperature, 24.0f);
  ASSERT_TRUE(this->ac_.fan_mode.has_value());
  EXPECT_EQ(this->ac_.fan_mode.value(), climate::ClimateFanMode::CLIMATE_FAN_HIGH);
  EXPECT_EQ(this->ac_.hlink_entity_status_.model_name, "RAK-25PEC");
  EXPECT_EQ(this->publish_count_, 1);
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
  EXPECT_EQ(this->ac_.state(), REQUEST_LOW_PRIORITY_FEATURE);

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

TEST_F(HlinkAcStateMachineTest, InvokesTimeoutCallbackAndResetsState) {
  bool timeout_called = false;
  this->ac_.set_low_priority_request_for_test({HlinkRequestFrame::Type::MT, {FeatureType::MODEL_NAME}}, nullptr,
                                              nullptr, nullptr, [&]() { timeout_called = true; });

  this->ac_.loop();
  ASSERT_EQ(this->ac_.state(), REQUEST_LOW_PRIORITY_FEATURE);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);
  EXPECT_EQ(this->uart_.take_tx_as_string(),
            "MT P=0900 C=FFF6\r");  // request MODEL_NAME

  this->ac_.advance_current_time_ms_for_test(301);
  this->ac_.loop();

  EXPECT_TRUE(timeout_called);
  EXPECT_EQ(this->ac_.state(), IDLE);
  EXPECT_EQ(this->ac_.status().current_request, nullptr);
  EXPECT_EQ(this->publish_count_, 0);
}

TEST_F(HlinkAcStateMachineTest, SetupInitSendsPowerStateRequestAndTransitionsToIdle) {
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "MT P=0000 C=FFFF\r");
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);

  this->inject_response_and_step("OK P=01 C=FFFE\r");
  EXPECT_EQ(this->ac_.state(), IDLE);
  EXPECT_EQ(this->publish_count_, 0);
}

TEST_F(HlinkAcStateMachineTest, SetupInitWithAcOffAppliesStoredTargetTemperatures) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);

  StoredTargetTemperatures stored_temps{};
  stored_temps.cool_target_temperature = 24.0f;
  this->ac_.set_stored_target_temperatures_for_test(stored_temps);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "MT P=0000 C=FFFF\r");
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);

  this->inject_response_and_step("OK P=00 C=FFFF\r");
  EXPECT_EQ(this->ac_.state(), APPLY_REQUEST);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0001,0040 C=FFBE\r");
  EXPECT_EQ(this->ac_.state(), ACK_APPLIED_REQUEST);

  this->inject_response_and_step(ACK_OK_FRAME);
  EXPECT_EQ(this->ac_.state(), APPLY_REQUEST);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "ST P=0003,0018 C=FFE4\r");
  EXPECT_EQ(this->ac_.state(), ACK_APPLIED_REQUEST);

  this->inject_response_and_step(ACK_OK_FRAME);
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);
  EXPECT_EQ(this->publish_count_, 0);
}

TEST_F(HlinkAcStateMachineTest, SetupInitWithAcOffAndNoStoredTargetTemperaturesDoesNothing) {
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "MT P=0000 C=FFFF\r");
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);

  this->inject_response_and_step("OK P=00 C=FFFF\r");
  EXPECT_EQ(this->ac_.state(), IDLE);
  EXPECT_TRUE(this->uart_.take_tx_as_string().empty());
  EXPECT_EQ(this->publish_count_, 0);
}

TEST_F(HlinkAcStateMachineTest, SetupInitWithAcOffAndRememberDisabledDoesNotApplyStoredTargetTemperatures) {
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);

  StoredTargetTemperatures stored_temps{};
  stored_temps.cool_target_temperature = 24.0f;
  this->ac_.set_stored_target_temperatures_for_test(stored_temps);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "MT P=0000 C=FFFF\r");
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);

  this->inject_response_and_step("OK P=00 C=FFFF\r");
  EXPECT_EQ(this->ac_.state(), IDLE);
  EXPECT_TRUE(this->uart_.take_tx_as_string().empty());
  EXPECT_EQ(this->publish_count_, 0);
}

TEST_F(HlinkAcStateMachineTest, ControlCapturesTargetTemperaturePerMode) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.set_reference_temperature(23);
  this->ac_.setup();

  auto call = this->ac_.make_call();
  call.set_mode(climate::ClimateMode::CLIMATE_MODE_COOL).set_target_temperature(24.0f);
  this->ac_.control(call);

  auto stored_temps = this->ac_.stored_target_temperatures_for_test();
  ASSERT_TRUE(stored_temps.cool_target_temperature.has_value());
  EXPECT_FLOAT_EQ(stored_temps.cool_target_temperature.value(), 24.0f);
  EXPECT_FALSE(stored_temps.heat_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.heat_cool_target_temperature.has_value());
  EXPECT_FALSE(stored_temps.dry_target_temperature.has_value());

  // Auto-mode temperature is clamped and stored in the auto range.
  auto auto_call = this->ac_.make_call();
  auto_call.set_mode(climate::ClimateMode::CLIMATE_MODE_HEAT_COOL).set_target_temperature(28.0f);
  this->ac_.control(auto_call);

  stored_temps = this->ac_.stored_target_temperatures_for_test();
  ASSERT_TRUE(stored_temps.heat_cool_target_temperature.has_value());
  EXPECT_FLOAT_EQ(stored_temps.heat_cool_target_temperature.value(), 26.0f);
  EXPECT_FLOAT_EQ(stored_temps.cool_target_temperature.value(), 24.0f);

  // OFF mode does not capture a target temperature.
  auto off_call = this->ac_.make_call();
  off_call.set_mode(climate::ClimateMode::CLIMATE_MODE_OFF).set_target_temperature(22.0f);
  this->ac_.control(off_call);

  stored_temps = this->ac_.stored_target_temperatures_for_test();
  EXPECT_FLOAT_EQ(stored_temps.cool_target_temperature.value(), 24.0f);
  EXPECT_FALSE(stored_temps.heat_target_temperature.has_value());
}

TEST_F(HlinkAcStateMachineTest, ControlDoesNotCaptureWhenRememberDisabled) {
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

TEST_F(HlinkAcStateMachineTest, PersistsTargetTemperaturesAcrossRestarts) {
  this->ac_.set_remember_target_temperatures(true);
  this->ac_.setup();
  auto call = this->ac_.make_call();
  call.set_mode(climate::ClimateMode::CLIMATE_MODE_HEAT).set_target_temperature(26.0f);
  this->ac_.control(call);

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

TEST_F(HlinkAcStateMachineTest, SetupInitTimeoutResetsToIdle) {
  this->ac_.setup();
  ASSERT_EQ(this->ac_.state(), INIT);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), "MT P=0000 C=FFFF\r");
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);

  this->ac_.advance_current_time_ms_for_test(301);
  this->ac_.loop();

  EXPECT_EQ(this->ac_.state(), IDLE);
  EXPECT_EQ(this->ac_.status().current_request, nullptr);
  EXPECT_EQ(this->publish_count_, 0);
}

}  // namespace esphome::hlink_ac::testing
