#pragma once

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "esphome/components/hlink_ac/hlink_ac.h"

namespace esphome::hlink_ac::testing {

inline std::string to_hex_u16(uint16_t value) {
  std::ostringstream oss;
  oss << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << value;
  return oss.str();
}

inline std::string to_hex_bytes(const std::vector<uint8_t> &bytes) {
  std::ostringstream oss;
  oss << std::uppercase << std::hex << std::setfill('0');
  for (const auto &byte : bytes) {
    oss << std::setw(2) << static_cast<uint16_t>(byte);
  }
  return oss.str();
}

inline std::string build_request_frame_string(const HlinkRequestFrame &frame) {
  uint16_t checksum = 0xFFFF;
  checksum -= static_cast<uint8_t>((frame.p.address >> 8) & 0xFF);
  checksum -= static_cast<uint8_t>(frame.p.address & 0xFF);
  if (frame.p.data.has_value()) {
    for (const auto &byte : frame.p.data.value()) {
      checksum -= byte;
    }
  }
  const std::string type = frame.type == HlinkRequestFrame::Type::MT ? "MT" : "ST";
  if (frame.p.data.has_value()) {
    return type + " P=" + to_hex_u16(frame.p.address) + "," + to_hex_bytes(frame.p.data.value()) +
           " C=" + to_hex_u16(checksum) + "\r";
  }
  return type + " P=" + to_hex_u16(frame.p.address) + " C=" + to_hex_u16(checksum) + "\r";
}

inline std::string build_response_frame_string(const char *status, const std::vector<uint8_t> &payload) {
  uint16_t checksum = 0xFFFF;
  for (const auto &byte : payload) {
    checksum -= byte;
  }
  return std::string(status) + " P=" + to_hex_bytes(payload) + " C=" + to_hex_u16(checksum) + "\r";
}

inline const std::string ACK_OK_FRAME = "OK\r";

}  // namespace esphome::hlink_ac::testing
