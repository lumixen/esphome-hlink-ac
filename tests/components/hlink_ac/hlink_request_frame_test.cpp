#include "common.h"

namespace esphome::hlink_ac::testing {

TEST(HlinkRequestFrameTest, WithUint8) {
  auto frame = HlinkRequestFrame::with_uint8(HlinkRequestFrame::Type::ST, 0x0001, 0x42);

  EXPECT_EQ(frame.type, HlinkRequestFrame::Type::ST);
  EXPECT_EQ(frame.p.address, 0x0001);
  ASSERT_TRUE(frame.p.data.has_value());
  ASSERT_EQ(frame.p.data.value().size(), 1);
  EXPECT_EQ(frame.p.data.value()[0], 0x42);
}

TEST(HlinkRequestFrameTest, WithUint8MtType) {
  auto frame = HlinkRequestFrame::with_uint8(HlinkRequestFrame::Type::MT, 0x0100, 0x00);

  EXPECT_EQ(frame.type, HlinkRequestFrame::Type::MT);
  EXPECT_EQ(frame.p.address, 0x0100);
  ASSERT_TRUE(frame.p.data.has_value());
  EXPECT_EQ(frame.p.data.value()[0], 0x00);
}

TEST(HlinkRequestFrameTest, WithUint16BigEndian) {
  auto frame = HlinkRequestFrame::with_uint16(HlinkRequestFrame::Type::ST, 0x0003, 0x1234);

  ASSERT_TRUE(frame.p.data.has_value());
  ASSERT_EQ(frame.p.data.value().size(), 2);
  EXPECT_EQ(frame.p.data.value()[0], 0x12);
  EXPECT_EQ(frame.p.data.value()[1], 0x34);
}

TEST(HlinkRequestFrameTest, WithUint16Zero) {
  auto frame = HlinkRequestFrame::with_uint16(HlinkRequestFrame::Type::ST, 0x0000, 0x0000);

  ASSERT_TRUE(frame.p.data.has_value());
  ASSERT_EQ(frame.p.data.value().size(), 2);
  EXPECT_EQ(frame.p.data.value()[0], 0x00);
  EXPECT_EQ(frame.p.data.value()[1], 0x00);
}

TEST(HlinkRequestFrameTest, WithUint16MaxValue) {
  auto frame = HlinkRequestFrame::with_uint16(HlinkRequestFrame::Type::ST, 0xFFFF, 0xFFFF);

  ASSERT_TRUE(frame.p.data.has_value());
  ASSERT_EQ(frame.p.data.value().size(), 2);
  EXPECT_EQ(frame.p.data.value()[0], 0xFF);
  EXPECT_EQ(frame.p.data.value()[1], 0xFF);
}

TEST(HlinkRequestFrameTest, WithString) {
  auto frame = HlinkRequestFrame::with_string(HlinkRequestFrame::Type::ST, 0x0300, "0102");

  ASSERT_TRUE(frame.p.data.has_value());
  ASSERT_EQ(frame.p.data.value().size(), 2);
  EXPECT_EQ(frame.p.data.value()[0], 0x01);
  EXPECT_EQ(frame.p.data.value()[1], 0x02);
}

TEST(HlinkRequestFrameTest, WithStringOddLength) {
  auto frame = HlinkRequestFrame::with_string(HlinkRequestFrame::Type::ST, 0x0000, "AABBCC");

  ASSERT_TRUE(frame.p.data.has_value());
  ASSERT_EQ(frame.p.data.value().size(), 3);
  EXPECT_EQ(frame.p.data.value()[0], 0xAA);
  EXPECT_EQ(frame.p.data.value()[1], 0xBB);
  EXPECT_EQ(frame.p.data.value()[2], 0xCC);
}

}  // namespace esphome::hlink_ac::testing