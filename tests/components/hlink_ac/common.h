#pragma once

#include <deque>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "esphome/components/hlink_ac/hlink_ac.h"

namespace esphome::hlink_ac::testing {

class MockUARTComponent : public uart::UARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override { this->tx_buffer_.insert(this->tx_buffer_.end(), data, data + len); }

  bool peek_byte(uint8_t *data) override {
    if (this->rx_buffer_.empty()) {
      return false;
    }
    *data = this->rx_buffer_.front();
    return true;
  }

  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx_buffer_.size() < len) {
      return false;
    }
    for (size_t i = 0; i < len; i++) {
      data[i] = this->rx_buffer_.front();
      this->rx_buffer_.pop_front();
    }
    return true;
  }

  size_t available() override { return this->rx_buffer_.size(); }

  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }

  void inject_rx(const std::string &payload) {
    for (const char c : payload) {
      this->rx_buffer_.push_back(static_cast<uint8_t>(c));
    }
  }

  void inject_rx(const std::vector<uint8_t> &payload) {
    for (const uint8_t c : payload) {
      this->rx_buffer_.push_back(c);
    }
  }

  std::string take_tx_as_string() {
    const std::string out(this->tx_buffer_.begin(), this->tx_buffer_.end());
    this->tx_buffer_.clear();
    return out;
  }

  void clear_tx() { this->tx_buffer_.clear(); }

 protected:
  void check_logger_conflict() override {}

#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif

  std::vector<uint8_t> tx_buffer_{};
  std::deque<uint8_t> rx_buffer_{};
};

class TestHlinkAc : public HlinkAc {
 public:
  using HlinkAc::is_auto_temperature_mode_;
  using HlinkAc::clamp_auto_temperature_;
  using HlinkAc::encode_auto_temperature_;
  using HlinkAc::is_nanable_equal_;
  using HlinkAc::hlink_entity_status_;

  void set_reference_temperature(float ref) { this->reference_temperature_ = ref; }

  void set_uart_parent_for_test(uart::UARTComponent *uart) { this->set_uart_parent(uart); }

  void set_current_time_ms_for_test(uint32_t current_time_ms) { this->current_time_ms_ = current_time_ms; }

  void advance_current_time_ms_for_test(uint32_t delta_ms) { this->current_time_ms_ += delta_ms; }

  void request_status_update_for_test() { this->request_status_update_(); }

  void enqueue_request_for_test(HlinkRequestFrame request_frame,
                                std::function<void(const HlinkResponseFrame &response)> ok_callback = nullptr,
                                std::function<void()> ng_callback = nullptr,
                                std::function<void()> invalid_callback = nullptr,
                                std::function<void()> timeout_callback = nullptr) {
    this->enqueue_request_(request_frame, ok_callback, ng_callback, invalid_callback, timeout_callback);
  }

  void set_low_priority_request_for_test(HlinkRequestFrame request_frame,
                                         std::function<void(const HlinkResponseFrame &response)> ok_callback = nullptr,
                                         std::function<void()> ng_callback = nullptr,
                                         std::function<void()> invalid_callback = nullptr,
                                         std::function<void()> timeout_callback = nullptr) {
    this->status_.low_priority_hlink_request =
        HlinkRequest{request_frame, ok_callback, ng_callback, invalid_callback, timeout_callback};
  }

  HlinkComponentState state() const { return this->status_.state; }

  ComponentStatus &status() { return this->status_; }

 protected:
  uint32_t current_time_ms() const override { return this->current_time_ms_; }

  uint32_t current_time_ms_{1000};
};

}  // namespace esphome::hlink_ac::testing
