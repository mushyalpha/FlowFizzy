#include <gtest/gtest.h>

#define private public
#include "hardware/FlowMeter.h"
#undef private

TEST(FlowMeterTest, InitializesWithoutException) {
    FlowMeter fm(4, 17, 1.0f);
    // Don't init() if we don't want hardware/mock threads running, but we can anyway.
    EXPECT_TRUE(fm.init());
    fm.shutdown();
}

TEST(FlowMeterTest, StartsWithZeroVolume) {
    FlowMeter fm(4, 18, 1.0f);
    EXPECT_EQ(fm.getPulseCount(), 0);
    EXPECT_DOUBLE_EQ(fm.getVolumeML(), 0.0);
}

TEST(FlowMeterTest, CountsPulsesAndCalculatesVolume) {
    FlowMeter fm(4, 19, 2.5f); // 2.5 ml per pulse
    
    // Send fake pulse counts explicitly
    fm.pulseCount_.store(4);
    
    EXPECT_EQ(fm.getPulseCount(), 4);
    EXPECT_DOUBLE_EQ(fm.getVolumeML(), 10.0);
}

TEST(FlowMeterTest, ResetClearsCount) {
    FlowMeter fm(4, 20, 1.0f);
    
    // Send fake pulse count
    fm.pulseCount_.store(10);
    EXPECT_EQ(fm.getPulseCount(), 10);
    
    fm.resetCount();
    EXPECT_EQ(fm.getPulseCount(), 0);
    EXPECT_DOUBLE_EQ(fm.getVolumeML(), 0.0);
}

TEST(FlowMeterTest, DetectsReachedTarget) {
    FlowMeter fm(4, 21, 5.0f); // 5 ml per pulse
    
    fm.pulseCount_.store(2); // 10 ml
    EXPECT_FALSE(fm.hasReachedTarget(15.0)); // 10 < 15
    
    fm.pulseCount_.store(3); // 15 ml
    EXPECT_TRUE(fm.hasReachedTarget(15.0));  // 15 >= 15
}
