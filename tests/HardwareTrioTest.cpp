#include "PinConfig.h"
#include "hardware/PumpController.h"
#include "hardware/FlowMeter.h"
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

    std::cout << "=== Touchless Dispenser Gesture Terminal Test ===\n";
    std::cout << "ML per pulse  : " << ML_PER_PULSE << "\n\n";

    // ── Construct ─────────────────────────────────────────────────────────────
    PumpController pump(GPIO_CHIP_NO, PUMP_PIN);
    FlowMeter      flow(GPIO_CHIP_NO, FLOW_PIN, static_cast<float>(ML_PER_PULSE));
    GestureSensor  gesture; // defaults to bus 1, addr 0x39, thresh 100

    // ── Initialise ────────────────────────────────────────────────────────────
    if (!pump.init()) {
        std::cerr << "ERROR: PumpController failed to init\n"; return 1;
    }
    if (!flow.init()) {
        std::cerr << "ERROR: FlowMeter failed to init\n";
        pump.shutdown(); return 1;
    }
    if (!gesture.init()) {
        std::cerr << "ERROR: GestureSensor failed to init\n";
        pump.shutdown(); flow.shutdown(); return 1;
    }

    std::atomic<bool> isDispensing(false);
    std::atomic<bool> targetReached(false);
    bool waitingMessagePrinted = false;

    // McDonalds UK Sizes!
    std::atomic<int> activeTargetVolume(400); // Default to Medium

    gesture.registerEventCallback([&](const GestureEvent& ev) {
        
        // 1. Gesture Size Selection Logic
        if (ev.direction == GestureDir::LEFT) {
            if (activeTargetVolume == 500) activeTargetVolume = 400; // L -> M
            else if (activeTargetVolume == 400) activeTargetVolume = 250; // M -> S
            
            std::cout << "\n<<< SWIPE LEFT: Size decreased to ";
            if (activeTargetVolume == 250) std::cout << "SMALL (250ml)\n";
            else std::cout << "MEDIUM (400ml)\n";
            waitingMessagePrinted = false;
        } 
        else if (ev.direction == GestureDir::RIGHT) {
            if (activeTargetVolume == 250) activeTargetVolume = 400; // S -> M
            else if (activeTargetVolume == 400) activeTargetVolume = 500; // M -> L
            
            std::cout << "\n>>> SWIPE RIGHT: Size increased to ";
            if (activeTargetVolume == 500) std::cout << "LARGE (500ml)\n";
            else std::cout << "MEDIUM (400ml)\n";
            waitingMessagePrinted = false;
        }

        // 2. Proximity Dispensing Logic
        if (ev.state == ProximityState::PROXIMITY_TRIGGERED) {
            std::cout << "\n>>> CUP DETECTED! Dispensing " << activeTargetVolume << " ml.\n";
            if (!isDispensing && !targetReached) {
                flow.resetCount();
                isDispensing = true;
                waitingMessagePrinted = false;
                pump.turnOn();
            }
        } 
        else if (ev.state == ProximityState::PROXIMITY_CLEARED) {
            std::cout << "\n>>> CUP REMOVED! Dispensing stopped.\n";
            isDispensing = false;
            targetReached = false; // reset for next cup
            waitingMessagePrinted = false;
            pump.turnOff();
        }
    });

    std::cout << "\n[-] System idle. Swipe Left/Right to select size.\n";
    std::cout << "[-] Current Size: MEDIUM (400ml). Place cup to dispense...\n";

    // ── Live volume loop ──────────────────────────────────────────────────────
    while (keepRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        if (isDispensing) {
            double vol = flow.getVolumeML();
            int pulses = flow.getPulseCount();

            std::cout << "\r  [Pump RUNNING] Pulses: " << std::setw(5) << pulses
                      << "   Vol: " << std::fixed << std::setprecision(1)
                      << std::setw(7) << vol << " ml"
                      << std::flush;

            // Stop when we hit the dynamically selected size
            if (flow.hasReachedTarget((double)activeTargetVolume)) {
                std::cout << "\n\n*** TARGET " << activeTargetVolume << "ml REACHED! ***\n";
                pump.turnOff();
                isDispensing = false;
                targetReached = true;
                waitingMessagePrinted = false;
            }
        } else if (targetReached) {
            if (!waitingMessagePrinted) {
                std::cout << "\n[-] Please remove your full cup.\n";
                waitingMessagePrinted = true;
            }
        } else {
            // Idle but no target reached and not dispensing
            if (!waitingMessagePrinted) {
                std::cout << "\n[-] System idle. Swipe Left/Right to change size or place cup...\n";
                waitingMessagePrinted = true;
            }
        }
    }

cleanup:
    pump.turnOff();
    pump.shutdown();
    flow.shutdown();
    gesture.shutdown();

    std::cout << "\nTest complete.\n";
    return 0;
}
