#include "PinConfig.h"
#include "hardware/GestureSensor.h"
#include "hardware/PumpController.h"
#include "hardware/FlowMeter.h"
#include "hardware/LcdDisplay.h"
#include "state/FillingController.h"
#include "monitor/Monitor.h"
#include "utils/Timer.h"
#include "utils/Logger.h"

#include <csignal>
#include <signal.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <thread>

// ─────────────────────────────────────────────────────────────────────────────

int main() {
    // ── Construct hardware drivers ────────────────────────────────────────────
    GestureSensor    gestureSensor(GESTURE_I2C_BUS, GESTURE_I2C_ADDR, GESTURE_THRESHOLD);
    PumpController   pump(GPIO_CHIP_NO, PUMP_PIN);
    FlowMeter        flowMeter(GPIO_CHIP_NO, FLOW_PIN, static_cast<float>(ML_PER_PULSE));
    LcdDisplay       lcd(LCD_I2C_BUS, LCD_I2C_ADDRESS);

    // ── Initialise hardware ───────────────────────────────────────────────────
    if (!gestureSensor.init()) {
        Logger::error("Failed to initialise GestureSensor");
        return 1;
    }
    if (!pump.init()) {
        Logger::error("Failed to initialise PumpController");
        gestureSensor.shutdown();
        return 1;
    }
    if (!flowMeter.init()) {
        Logger::error("Failed to initialise FlowMeter");
        pump.shutdown();
        gestureSensor.shutdown();
        return 1;
    }
    if (!lcd.init()) {
        // LCD failure is non-fatal — the machine still works without a display
        Logger::error("Failed to initialise LcdDisplay — continuing without display");
    }

    // ── State machine + monitor ───────────────────────────────────────────────
    // FillingController registers itself as a GestureSensor proximity callback
    // internally — no polling, fully event-driven.
    FillingController controller(gestureSensor, pump, flowMeter,
                                 HOLD_TIME_SECONDS,
                                 TARGET_VOLUME_ML);

    Monitor monitor;
    controller.registerMonitor([&](const std::string& state, double vol, int bottles) {
        monitor.onStateChange(state, vol, bottles);
        lcd.print(0, state);
        std::ostringstream row1;
        if (state == "FILLING") {
            row1 << "Vol: " << std::fixed << std::setprecision(1) << vol << " ml";
        } else {
            row1 << "Bottles: " << bottles;
        }
        lcd.print(1, row1.str());
    });

    Logger::info("=== AquaFlow Filling Machine Started ===");
    Logger::info("Hold time     : " + std::to_string(HOLD_TIME_SECONDS) + " s");
    Logger::info("Target volume : " + std::to_string(TARGET_VOLUME_ML) + " ml");
    Logger::info("Gesture sensor: I2C bus " + std::to_string(GESTURE_I2C_BUS)
                 + ", addr 0x" + "39");

    // ── RTES event-driven loop: timerfd fires tick(), no polling ─────────────
    // The Timer blocks on a timerfd internally. controller.tick() is called as
    // a callback — main() never polls anything, and the GestureSensor proximity
    // events are delivered via a separate callback on its own worker thread.
    Timer loopTimer(LOOP_INTERVAL_MS);
    loopTimer.registerCallback([&]() {
        controller.tick();
        // Real-time volume update on the LCD (reads atomic FlowMeter counter)
        std::ostringstream row;
        row << "Vol: " << std::fixed << std::setprecision(1) << flowMeter.getVolumeML() << " ml";
        lcd.print(1, row.str());
    });
    loopTimer.start();

    // ── Block main thread until SIGINT (Ctrl+C) via sigwait ──────────────────
    // sigwait() is a POSIX blocking call — the kernel puts this thread to
    // sleep and wakes it only when a signal arrives.  No polling, no busy loop.
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &sigset, nullptr);   // route signals to sigwait

    int sig = 0;
    sigwait(&sigset, &sig);   // blocks here until Ctrl+C or kill
    Logger::info("Signal " + std::to_string(sig) + " received — shutting down.");

    // ── Shutdown (reverse init order) ────────────────────────────────────────
    loopTimer.stop();

    // Display farewell message before shutting down LCD
    lcd.clear();
    lcd.print(0, "AquaFlow");
    lcd.print(1, "Shutting down...");
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    lcd.shutdown();

    flowMeter.shutdown();
    pump.shutdown();
    gestureSensor.shutdown();

    Logger::info("AquaFlow stopped. Goodbye.");
    return 0;
}
