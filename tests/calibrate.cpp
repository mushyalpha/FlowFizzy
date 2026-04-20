/**
 * @file calibrate.cpp
 * @brief Standalone flow-meter calibration tool.
 *
 * Usage:
 *   sudo ./calibrate
 *
 * The pump runs until you press Ctrl+C.
 * Fill a measuring jug to a known volume (e.g. exactly 250 ml),
 * then press Ctrl+C.  Enter the actual volume when prompted and
 * the tool prints the correct ML_PER_PULSE value for PinConfig.h.
 */

#include "hardware/PumpController.h"
#include "hardware/FlowMeter.h"
#include "PinConfig.h"
#include "utils/Logger.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

static std::atomic<bool> g_stop{false};

static void onSignal(int) { g_stop.store(true); }

int main() {
    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);

    PumpController pump(GPIO_CHIP_NO, PUMP_PIN, PumpController::DriveMode::TRANSISTOR_ACTIVE_HIGH);
    FlowMeter      meter(GPIO_CHIP_NO, FLOW_PIN);

    if (!pump.init()) {
        Logger::error("Pump init failed — is another process holding GPIO " +
                      std::to_string(PUMP_PIN) + "?");
        return 1;
    }
    if (!meter.init()) {
        Logger::error("FlowMeter init failed — is another process holding GPIO " +
                      std::to_string(FLOW_PIN) + "?");
        return 1;
    }

    meter.resetCount();
    pump.turnOn();

    std::cout << "\n========================================\n"
              << "  AquaFlow Flow-Meter Calibration\n"
              << "========================================\n"
              << "  Pump is running.\n"
              << "  Fill a measuring jug to your target volume.\n"
              << "  Press Ctrl+C when the jug is full.\n"
              << "----------------------------------------\n";

    // Live volume display
    while (!g_stop.load()) {
        double vol = meter.getVolumeML();
        std::cout << "\r  Current reading : "
                  << std::fixed
                  << vol << " ml   (raw pulses: " << meter.getPulseCount() << ")"
                  << "    " << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    pump.turnOff();

    long   pulses   = meter.getPulseCount();
    double reported = meter.getVolumeML();

    std::cout << "\n\n----------------------------------------\n"
              << "  Pump stopped.\n"
              << "  System reported : " << reported << " ml\n"
              << "  Raw pulse count : " << pulses   << "\n"
              << "----------------------------------------\n"
              << "  Enter the ACTUAL volume in the jug (ml): ";

    double actual = 0.0;
    std::cin >> actual;

    if (pulses > 0 && actual > 0.0) {
        double newFactor = actual / static_cast<double>(pulses);
        std::cout << "\n  ✓ New ML_PER_PULSE = " << newFactor << "\n\n"
                  << "  Update include/PinConfig.h:\n"
                  << "    constexpr double ML_PER_PULSE = " << newFactor << ";\n\n"
                  << "  Old factor was : " << ML_PER_PULSE << "\n"
                  << "  Correction     : ×" << (newFactor / ML_PER_PULSE) << "\n";
    } else {
        std::cout << "  No pulses recorded — check GPIO " << FLOW_PIN << " wiring.\n";
    }

    return 0;
}
