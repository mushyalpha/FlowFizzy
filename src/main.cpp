#include "PinConfig.h"
#include "hardware/UltrasonicSensor.h"
#include "hardware/PumpController.h"
#include "hardware/FlowMeter.h"
#include "state/FillingController.h"
#include "monitor/Monitor.h"
#include "utils/Timer.h"
#include "utils/Logger.h"

#include <csignal>
#include <signal.h>

// ─────────────────────────────────────────────────────────────────────────────

int main() {
    // ── Construct hardware drivers (all use libgpiod internally) ─────────────
    UltrasonicSensor sensor(GPIO_CHIP_NO, TRIG_PIN, ECHO_PIN, SENSOR_PERIOD_MS);
    PumpController   pump(GPIO_CHIP_NO, PUMP_PIN);
    FlowMeter        flowMeter(GPIO_CHIP_NO, FLOW_PIN, static_cast<float>(ML_PER_PULSE));

    // ── Initialise hardware ───────────────────────────────────────────────────
    if (!sensor.init()) {
        Logger::error("Failed to initialise UltrasonicSensor");
        return 1;
    }
    if (!pump.init()) {
        Logger::error("Failed to initialise PumpController");
        sensor.shutdown();
        return 1;
    }
    if (!flowMeter.init()) {
        Logger::error("Failed to initialise FlowMeter");
        pump.shutdown();
        sensor.shutdown();
        return 1;
    }

    // ── State machine + monitor ───────────────────────────────────────────────
    FillingController controller(sensor, pump, flowMeter,
                                 TARGET_DISTANCE_CM,
                                 DISTANCE_TOLERANCE_CM,
                                 HOLD_TIME_SECONDS,
                                 TARGET_VOLUME_ML);

    Monitor monitor;
    controller.registerMonitor([&](const std::string& state, double vol, int bottles) {
        monitor.onStateChange(state, vol, bottles);
    });

    Logger::info("=== AquaFlow Filling Machine Started ===");
    Logger::info("Target distance : " + std::to_string(TARGET_DISTANCE_CM) + " cm");
    Logger::info("Tolerance       : +-" + std::to_string(DISTANCE_TOLERANCE_CM) + " cm");
    Logger::info("Hold time       : " + std::to_string(HOLD_TIME_SECONDS) + " s");
    Logger::info("Target volume   : " + std::to_string(TARGET_VOLUME_ML) + " ml");

    // ── RTES event-driven loop: timerfd fires tick(), no polling ─────────────
    // The Timer uses a timerfd worker thread internally.  controller.tick()
    // is called as a callback — main() never polls anything.
    Timer loopTimer(LOOP_INTERVAL_MS);
    loopTimer.registerCallback([&]() {
        controller.tick();
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

    // ── Shutdown ──────────────────────────────────────────────────────────────
    loopTimer.stop();
    flowMeter.shutdown();
    pump.shutdown();
    sensor.shutdown();

    Logger::info("AquaFlow stopped. Goodbye.");
    return 0;
}
