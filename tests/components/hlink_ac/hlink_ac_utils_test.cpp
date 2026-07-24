#include "common.h"

namespace esphome::hlink_ac::testing {

class HlinkAcUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ac_.set_reference_temperature(25.0f);
  }

  TestHlinkAc ac_;
};

TEST_F(HlinkAcUtilsTest, IsAutoTemperatureModeAuto) {
  EXPECT_TRUE(ac_.is_auto_temperature_mode_(HLINK_MODE_AUTO));
}

TEST_F(HlinkAcUtilsTest, IsAutoTemperatureModeHeatAuto) {
  EXPECT_TRUE(ac_.is_auto_temperature_mode_(HLINK_MODE_HEAT_AUTO));
}

TEST_F(HlinkAcUtilsTest, IsAutoTemperatureModeCoolAuto) {
  EXPECT_TRUE(ac_.is_auto_temperature_mode_(HLINK_MODE_COOL_AUTO));
}

TEST_F(HlinkAcUtilsTest, IsAutoTemperatureModeDryAuto) {
  EXPECT_TRUE(ac_.is_auto_temperature_mode_(HLINK_MODE_DRY_AUTO));
}

TEST_F(HlinkAcUtilsTest, IsAutoTemperatureModeHeatReturnsFalse) {
  EXPECT_FALSE(ac_.is_auto_temperature_mode_(HLINK_MODE_HEAT));
}

TEST_F(HlinkAcUtilsTest, IsAutoTemperatureModeCoolReturnsFalse) {
  EXPECT_FALSE(ac_.is_auto_temperature_mode_(HLINK_MODE_COOL));
}

TEST_F(HlinkAcUtilsTest, IsAutoTemperatureModeDryReturnsFalse) {
  EXPECT_FALSE(ac_.is_auto_temperature_mode_(HLINK_MODE_DRY));
}

TEST_F(HlinkAcUtilsTest, IsAutoTemperatureModeFanReturnsFalse) {
  EXPECT_FALSE(ac_.is_auto_temperature_mode_(HLINK_MODE_FAN));
}

TEST_F(HlinkAcUtilsTest, IsAutoTemperatureModeZeroReturnsFalse) {
  EXPECT_FALSE(ac_.is_auto_temperature_mode_(0x0000));
}

TEST_F(HlinkAcUtilsTest, ClampAutoTemperatureBelowMin) {
  float result = ac_.clamp_auto_temperature_(20.0f);
  EXPECT_FLOAT_EQ(result, 22.0f);  // ref (25) + delta_min (-3)
}

TEST_F(HlinkAcUtilsTest, ClampAutoTemperatureAboveMax) {
  float result = ac_.clamp_auto_temperature_(30.0f);
  EXPECT_FLOAT_EQ(result, 28.0f);  // ref (25) + delta_max (+3)
}

TEST_F(HlinkAcUtilsTest, ClampAutoTemperatureInRange) {
  float result = ac_.clamp_auto_temperature_(26.0f);
  EXPECT_FLOAT_EQ(result, 26.0f);
}

TEST_F(HlinkAcUtilsTest, ClampAutoTemperatureAtMin) {
  float result = ac_.clamp_auto_temperature_(22.0f);
  EXPECT_FLOAT_EQ(result, 22.0f);
}

TEST_F(HlinkAcUtilsTest, ClampAutoTemperatureAtMax) {
  float result = ac_.clamp_auto_temperature_(28.0f);
  EXPECT_FLOAT_EQ(result, 28.0f);
}

TEST_F(HlinkAcUtilsTest, EncodeAutoTemperatureZeroOffset) {
  uint16_t result = ac_.encode_auto_temperature_(25.0f);
  EXPECT_EQ(result, 0xFF00);
}

TEST_F(HlinkAcUtilsTest, EncodeAutoTemperaturePositiveOffset) {
  uint16_t result = ac_.encode_auto_temperature_(27.0f);
  EXPECT_EQ(result, 0xFF02);
}

TEST_F(HlinkAcUtilsTest, EncodeAutoTemperatureNegativeOffset) {
  uint16_t result = ac_.encode_auto_temperature_(23.0f);
  EXPECT_EQ(result, static_cast<uint16_t>(0xFFFE));
}

TEST_F(HlinkAcUtilsTest, EncodeAutoTemperatureMaxOffset) {
  uint16_t result = ac_.encode_auto_temperature_(28.0f);
  EXPECT_EQ(result, 0xFF03);
}

TEST_F(HlinkAcUtilsTest, EncodeAutoTemperatureMinOffset) {
  uint16_t result = ac_.encode_auto_temperature_(22.0f);
  EXPECT_EQ(result, static_cast<uint16_t>(0xFFFD));
}

TEST_F(HlinkAcUtilsTest, IsNanableEqualBothNan) {
  EXPECT_TRUE(ac_.is_nanable_equal_(NAN, NAN));
}

TEST_F(HlinkAcUtilsTest, IsNanableEqualEqualValues) {
  EXPECT_TRUE(ac_.is_nanable_equal_(25.0f, 25.0f));
}

TEST_F(HlinkAcUtilsTest, IsNanableEqualDifferentValues) {
  EXPECT_FALSE(ac_.is_nanable_equal_(25.0f, 26.0f));
}

TEST_F(HlinkAcUtilsTest, IsNanableEqualOneNan) {
  EXPECT_FALSE(ac_.is_nanable_equal_(NAN, 25.0f));
  EXPECT_FALSE(ac_.is_nanable_equal_(25.0f, NAN));
}

TEST_F(HlinkAcUtilsTest, FormatTargetTemperatureLogNoValue) {
  auto result = ac_.format_target_temperature_log_({}, false);
  EXPECT_EQ(result, "N/A");
}

TEST_F(HlinkAcUtilsTest, FormatTargetTemperatureLogNan) {
  auto result = ac_.format_target_temperature_log_(NAN, false);
  EXPECT_EQ(result, "N/A");
}

TEST_F(HlinkAcUtilsTest, FormatTargetTemperatureLogNormal) {
  auto result = ac_.format_target_temperature_log_(25.0f, false);
  EXPECT_EQ(result, "25");
}

TEST_F(HlinkAcUtilsTest, FormatTargetTemperatureLogWithAutoOffset) {
  auto result = ac_.format_target_temperature_log_(27.0f, true);
  EXPECT_EQ(result, "27 (auto offset 2)");
}

TEST_F(HlinkAcUtilsTest, FormatTargetTemperatureLogWithNegativeAutoOffset) {
  auto result = ac_.format_target_temperature_log_(23.0f, true);
  EXPECT_EQ(result, "23 (auto offset -2)");
}

}  // namespace esphome::hlink_ac::testing