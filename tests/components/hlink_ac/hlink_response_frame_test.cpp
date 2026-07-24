#include "common.h"

namespace esphome::hlink_ac::testing {

TEST(HlinkResponseFrameTest, PValueAsUint16SingleByte) {
  HlinkResponseFrame frame{HlinkResponseFrame::Status::OK,
                           std::vector<uint8_t>{0xAB},
                           0x0000};
  auto result = frame.p_value_as_uint16();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 0xAB);
}

TEST(HlinkResponseFrameTest, PValueAsUint16TwoBytes) {
  HlinkResponseFrame frame{HlinkResponseFrame::Status::OK,
                           std::vector<uint8_t>{0x12, 0x34},
                           0x0000};
  auto result = frame.p_value_as_uint16();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 0x1234);
}

TEST(HlinkResponseFrameTest, PValueAsUint16Empty) {
  HlinkResponseFrame frame{HlinkResponseFrame::Status::OK, {}, 0x0000};
  auto result = frame.p_value_as_uint16();
  EXPECT_FALSE(result.has_value());
}

TEST(HlinkResponseFrameTest, PValueAsInt8Positive) {
  HlinkResponseFrame frame{HlinkResponseFrame::Status::OK,
                           std::vector<uint8_t>{0x7E},
                           0x0000};
  auto result = frame.p_value_as_int8();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 0x7E);
}

TEST(HlinkResponseFrameTest, PValueAsInt8Negative) {
  HlinkResponseFrame frame{HlinkResponseFrame::Status::OK,
                           std::vector<uint8_t>{0xFE},
                           0x0000};
  auto result = frame.p_value_as_int8();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), -2);
}

TEST(HlinkResponseFrameTest, PValueAsInt8WrongSize) {
  HlinkResponseFrame frame{HlinkResponseFrame::Status::OK,
                           std::vector<uint8_t>{0x01, 0x02},
                           0x0000};
  auto result = frame.p_value_as_int8();
  EXPECT_FALSE(result.has_value());
}

TEST(HlinkResponseFrameTest, PValueAsInt8Empty) {
  HlinkResponseFrame frame{HlinkResponseFrame::Status::OK, {}, 0x0000};
  auto result = frame.p_value_as_int8();
  EXPECT_FALSE(result.has_value());
}

TEST(HlinkResponseFrameTest, PValueAsStringSingleByte) {
  HlinkResponseFrame frame{HlinkResponseFrame::Status::OK,
                           std::vector<uint8_t>{0xAB},
                           0x0000};
  auto result = frame.p_value_as_string();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "AB");
}

TEST(HlinkResponseFrameTest, PValueAsStringMultipleBytes) {
  HlinkResponseFrame frame{HlinkResponseFrame::Status::OK,
                           std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF},
                           0x0000};
  auto result = frame.p_value_as_string();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "DEADBEEF");
}

TEST(HlinkResponseFrameTest, PValueAsStringEmpty) {
  HlinkResponseFrame frame{HlinkResponseFrame::Status::OK, {}, 0x0000};
  auto result = frame.p_value_as_string();
  EXPECT_FALSE(result.has_value());
}

}  // namespace esphome::hlink_ac::testing