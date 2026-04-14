#include <gtest/gtest.h>
#include "hardware/PumpController.h"
#include "hardware/FlowMeter.h"

// Allow testing access to private member eventCallback_ and bottlePresent_ in FillingController/GestureSensor
#define private public
#include "state/FillingController.h"
#include "hardware/GestureSensor.h"
#undef private

#include <thread>
#include <chrono>

class FillingControllerTest : public ::testing::Test {
protected:
    GestureSensor* gs;
    PumpController* pump;
    FlowMeter* flow;
    FillingController* fc;

    void SetUp() override {
        gs = new GestureSensor();
        pump = new PumpController(4, 27);
        flow = new FlowMeter(4, 17, 1.0f);
        
        // 2 seconds hold time, 50 ml target
        fc = new FillingController(*gs, *pump, *flow, 2, 50.0);
    }

    void TearDown() override {
        delete fc;
        delete flow;
        delete pump;
        delete gs;
    }

    // Helper to simulate a gesture sensor event
    void simulateProximity(ProximityState state) {
        GestureEvent ev;
        ev.state = state;
        ev.direction = GestureDir::NONE;
        ev.proximityValue = 200;
        
        if (gs->eventCallback_) {
            gs->eventCallback_(ev);
        }
    }
};

TEST_F(FillingControllerTest, InitialStateIsWaiting) {
    EXPECT_EQ(fc->getState(), SystemState::WAITING);
    EXPECT_EQ(fc->getBottleCount(), 0);
}

TEST_F(FillingControllerTest, TransitionToConfirmationOnProximity) {
    EXPECT_EQ(fc->getState(), SystemState::WAITING);
    
    simulateProximity(ProximityState::PROXIMITY_TRIGGERED);
    fc->tick(); // Tick to process state
    
    EXPECT_EQ(fc->getState(), SystemState::CONFIRMATION);
}

TEST_F(FillingControllerTest, ReturnsToWaitingIfProximityClearedPrematurely) {
    simulateProximity(ProximityState::PROXIMITY_TRIGGERED);
    fc->tick();
    EXPECT_EQ(fc->getState(), SystemState::CONFIRMATION);
    
    // Clear proximity before hold time expires
    simulateProximity(ProximityState::PROXIMITY_CLEARED);
    fc->tick();
    
    EXPECT_EQ(fc->getState(), SystemState::WAITING);
}

TEST_F(FillingControllerTest, CompletesFullCycleSuccessfully) {
    simulateProximity(ProximityState::PROXIMITY_TRIGGERED);
    fc->tick();
    EXPECT_EQ(fc->getState(), SystemState::CONFIRMATION);
    
    // Fast forward hold time externally by hacking the startTime, 
    // or just sleep for hold time (2 seconds). Sleep is simpler for this test.
    std::this_thread::sleep_for(std::chrono::milliseconds(2100));
    
    fc->tick(); // Should transition to FILLING
    EXPECT_EQ(fc->getState(), SystemState::FILLING);
    EXPECT_TRUE(pump->isRunning()); // Pump should be ON
    
    // Simulate flow meter reaching target
    // In our test it's looking for 50ml. Let's hijack the flow meter count directly since we use friend access.
    flow->pulseCount_.store(50); 
    
    fc->tick(); // Should transition to FILL_COMPLETE
    EXPECT_EQ(fc->getState(), SystemState::FILL_COMPLETE);
    EXPECT_FALSE(pump->isRunning()); // Pump should be OFF
    EXPECT_EQ(fc->getBottleCount(), 1);
    
    fc->tick(); // Should transition back to WAITING
    EXPECT_EQ(fc->getState(), SystemState::WAITING);
}
