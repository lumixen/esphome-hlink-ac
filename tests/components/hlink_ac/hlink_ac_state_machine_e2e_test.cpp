#include "common.h"
#include "hlink_test_utils.h"

namespace esphome::hlink_ac::testing {

class HlinkAcStateMachineE2ETest : public ::testing::Test {
 protected:
  void advance_for_next_send() { this->ac_.advance_current_time_ms_for_test(MIN_INTERVAL_BETWEEN_REQUESTS + 1); }

  void SetUp() override {
    this->ac_.set_uart_parent_for_test(&this->uart_);
    this->ac_.set_current_time_ms_for_test(0);
    this->ac_.set_status_update_interval(0xFFFFFF00U);
    this->ac_.add_on_state_callback([this](climate::Climate &) { this->publish_count_++; });
  }

  void send_poll_request_and_assert(const HlinkRequestFrame &expected_frame) {
    this->advance_for_next_send();
    this->ac_.loop();
    EXPECT_EQ(this->uart_.take_tx_as_string(), build_request_frame_string(expected_frame));
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

TEST_F(HlinkAcStateMachineE2ETest, PollingCycleHappyPath) {
  this->ac_.request_status_update_for_test();
  ASSERT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->send_poll_request_and_assert({HlinkRequestFrame::Type::MT, {FeatureType::POWER_STATE}});
  this->inject_response_and_step(build_response_frame_string("OK", {0x01}));
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->send_poll_request_and_assert({HlinkRequestFrame::Type::MT, {FeatureType::MODE}});
  this->inject_response_and_step(build_response_frame_string("OK", {0x00, 0x40}));
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->send_poll_request_and_assert({HlinkRequestFrame::Type::MT, {FeatureType::TARGET_TEMP}});
  this->inject_response_and_step(build_response_frame_string("OK", {0x00, 0x16}));
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->send_poll_request_and_assert({HlinkRequestFrame::Type::MT, {FeatureType::CURRENT_INDOOR_TEMP}});
  this->inject_response_and_step(build_response_frame_string("OK", {0x00, 0x18}));
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);

  this->send_poll_request_and_assert({HlinkRequestFrame::Type::MT, {FeatureType::FAN_MODE}});
  this->inject_response_and_step(build_response_frame_string("OK", {HLINK_FAN_HIGH}));

  EXPECT_EQ(this->ac_.state(), IDLE);
  EXPECT_EQ(this->ac_.mode, climate::ClimateMode::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(this->ac_.target_temperature, 22.0f);
  EXPECT_FLOAT_EQ(this->ac_.current_temperature, 24.0f);
  ASSERT_TRUE(this->ac_.fan_mode.has_value());
  EXPECT_EQ(this->ac_.fan_mode.value(), climate::ClimateFanMode::CLIMATE_FAN_HIGH);
  EXPECT_EQ(this->publish_count_, 1);
}

TEST_F(HlinkAcStateMachineE2ETest, PartialResponseIsCompletedOnNextLoop) {
  this->ac_.request_status_update_for_test();
  this->send_poll_request_and_assert({HlinkRequestFrame::Type::MT, {FeatureType::POWER_STATE}});

  this->inject_response_and_step("OK P=01 C=FF");
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);

  this->inject_response_and_step("FE\r");
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);
  EXPECT_EQ(this->publish_count_, 0);
}

TEST_F(HlinkAcStateMachineE2ETest, SendsNextRequestOnlyAfterStrictlyMoreThanMinInterval) {
  this->ac_.request_status_update_for_test();
  this->send_poll_request_and_assert({HlinkRequestFrame::Type::MT, {FeatureType::POWER_STATE}});

  this->inject_response_and_step(build_response_frame_string("OK", {0x01}));
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
            build_request_frame_string({HlinkRequestFrame::Type::MT, {FeatureType::MODE}}));
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);
  EXPECT_EQ(this->publish_count_, 0);
}

TEST_F(HlinkAcStateMachineE2ETest, AppliesQueuedRequestsAndStartsStatusRefresh) {
  int ok_callbacks_called = 0;
  this->ac_.enqueue_request_for_test(HlinkRequestFrame::with_uint8(HlinkRequestFrame::Type::ST, FeatureType::POWER_STATE, 0x01),
                                     [&](const HlinkResponseFrame &) { ok_callbacks_called++; });
  this->ac_.enqueue_request_for_test(HlinkRequestFrame::with_uint16(HlinkRequestFrame::Type::ST, FeatureType::MODE, HLINK_MODE_COOL),
                                     [&](const HlinkResponseFrame &) { ok_callbacks_called++; });

  this->ac_.loop();
  EXPECT_EQ(this->ac_.state(), APPLY_REQUEST);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(),
            build_request_frame_string(HlinkRequestFrame::with_uint8(HlinkRequestFrame::Type::ST, FeatureType::POWER_STATE, 0x01)));
  EXPECT_EQ(this->ac_.state(), ACK_APPLIED_REQUEST);

  this->inject_response_and_step(ACK_OK_FRAME);
  EXPECT_EQ(this->ac_.state(), APPLY_REQUEST);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(), build_request_frame_string(
                                             HlinkRequestFrame::with_uint16(HlinkRequestFrame::Type::ST, FeatureType::MODE, HLINK_MODE_COOL)));
  EXPECT_EQ(this->ac_.state(), ACK_APPLIED_REQUEST);

  this->inject_response_and_step(ACK_OK_FRAME);
  EXPECT_EQ(this->ac_.state(), REQUEST_NEXT_STATUS_FEATURE);
  EXPECT_EQ(ok_callbacks_called, 2);
  EXPECT_EQ(this->publish_count_, 0);
}

TEST_F(HlinkAcStateMachineE2ETest, HandlesLowPriorityRequestFromIdle) {
  bool callback_called = false;
  std::string payload_string;
  this->ac_.set_low_priority_request_for_test(
      {HlinkRequestFrame::Type::MT, {FeatureType::MODEL_NAME}},
      [&](const HlinkResponseFrame &response) {
        callback_called = true;
        payload_string = response.p_value_as_string().value_or("");
      });

  this->ac_.loop();
  EXPECT_EQ(this->ac_.state(), REQUEST_LOW_PRIORITY_FEATURE);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->uart_.take_tx_as_string(),
            build_request_frame_string({HlinkRequestFrame::Type::MT, {FeatureType::MODEL_NAME}}));
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);

  this->inject_response_and_step(build_response_frame_string("OK", {0x41, 0x42}));
  EXPECT_EQ(this->ac_.state(), IDLE);
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(payload_string, "4142");
  EXPECT_EQ(this->publish_count_, 0);
}

TEST_F(HlinkAcStateMachineE2ETest, InvokesTimeoutCallbackAndResetsState) {
  bool timeout_called = false;
  this->ac_.set_low_priority_request_for_test(
      {HlinkRequestFrame::Type::MT, {FeatureType::MODEL_NAME}}, nullptr, nullptr, nullptr,
      [&]() { timeout_called = true; });

  this->ac_.loop();
  ASSERT_EQ(this->ac_.state(), REQUEST_LOW_PRIORITY_FEATURE);

  this->advance_for_next_send();
  this->ac_.loop();
  EXPECT_EQ(this->ac_.state(), READ_FEATURE_RESPONSE);
  EXPECT_EQ(this->uart_.take_tx_as_string(),
            build_request_frame_string({HlinkRequestFrame::Type::MT, {FeatureType::MODEL_NAME}}));

  this->ac_.advance_current_time_ms_for_test(301);
  this->ac_.loop();

  EXPECT_TRUE(timeout_called);
  EXPECT_EQ(this->ac_.state(), IDLE);
  EXPECT_EQ(this->ac_.status().current_request, nullptr);
  EXPECT_EQ(this->publish_count_, 0);
}

}  // namespace esphome::hlink_ac::testing
