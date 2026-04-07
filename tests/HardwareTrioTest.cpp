#include "PinConfig.h"
#include "hardware/PumpController.h"
#include "hardware/FlowMeter.h"
#include "hardware/LcdDisplay.h"
#include "hardware/GestureSensor.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <iomanip>
#include <thread>

// ─── Graceful shutdown on Ctrl+C ─────────────────────────────────────────────
static std::atomic<bool> keepRunning(true);

void signalHandler(int signum) {
    std::cout << "\n[Signal " << signum << "] Stopping...\n";
    keepRunning = false;
}

int main() {
    std::signal(SIGINT, signalHandler);

    std::cout << "=== Touchless Dispenser Integration Test ===\n";
    std::cout << "Target volume : " << TARGET_VOLUME_ML << " ml\n";
    std::cout << "ML per pulse  : " << ML_PER_PULSE << "\n\n";

    // ── Construct ─────────────────────────────────────────────────────────────
    PumpController pump(GPIO_CHIP_NO, PUMP_PIN);
    FlowMeter      flow(GPIO_CHIP_NO, FLOW_PIN, static_cast<float>(ML_PER_PULSE));
    LcdDisplay     lcd(LCD_I2C_BUS, LCD_I2C_ADDRESS);
    GestureSensor  gesture; // defaults to bus 1, addr 0x39, thresh 100

    // ── Initialise ────────────────────────────────────────────────────────────
    if (!pump.init()) {
        std::cerr << "ERROR: PumpController failed to init\n"; return 1;
    }
    if (!flow.init()) {
        std::cerr << "ERROR: FlowMeter failed to init\n";
        pump.shutdown(); return 1;
    }
    if (!lcd.init()) {
        std::cerr << "WARNING: LcdDisplay failed to init — continuing without display\n";
    }
    if (!gesture.init()) {
        std::cerr << "ERROR: GestureSensor failed to init\n";
        pump.shutdown(); flow.shutdown(); return 1;
    }

    std::atomic<bool> isDispensing(false);
    std::atomic<bool> targetReached(false);

    gesture.registerEventCallback([&](const GestureEvent& ev) {
        if (ev.state == ProximityState::PROXIMITY_TRIGGERED) {
            std::cout << "\nCup detected! Ready to dispense.\n";
            // Start pour only if we aren't already dispensing or haven't hit target yet
            if (!isDispensing && !targetReached) {
                flow.resetCount();
                isDispensing = true;
                pump.turnOn();
            }
        } else if (ev.state == ProximityState::PROXIMITY_CLEARED) {
            std::cout << "\nCup removed! Dispensing stopped.\n";
            isDispensing = false;
            targetReached = false; // reset for next cup
            pump.turnOff();
        }
    });

    lcd.print(0, "Place Cup");
    lcd.print(1, "to Dispense");

    // ── Live volume loop ──────────────────────────────────────────────────────
    while (keepRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        if (isDispensing) {
            double vol = flow.getVolumeML();
            int pulses = flow.getPulseCount();

            lcd.showVolume(vol);

            std::cout << "\r  Pulses: " << std::setw(5) << pulses
                      << "   Vol: " << std::fixed << std::setprecision(1)
                      << std::setw(7) << vol << " ml"
                      << std::flush;

            // Stop when target reached
            if (flow.hasReachedTarget(TARGET_VOLUME_ML)) {
                std::cout << "\nTarget reached!\n";
                pump.turnOff();
                isDispensing = false;
                targetReached = true;
                
                lcd.showStatus("DONE", vol, 1);
                
                // Show DONE for 2 seconds, then prompt for next cup if removed
                std::this_thread::sleep_for(std::chrono::seconds(2));
                lcd.print(0, "Remove Cup");
                lcd.print(1, "");
            }
        } else if (!targetReached) {
            // Idle state wait for cup
            lcd.print(0, "Place Cup");
            lcd.print(1, "to Dispense");
        }
    }

cleanup:
    pump.turnOff();
    pump.shutdown();
    flow.shutdown();
    lcd.shutdown();
    gesture.shutdown();

    std::cout << "\nTest complete.\n";
    return 0;
}
