#pragma once

#include "IProximitySensor.h"
#include "IPump.h"
#include "IFlowMeter.h"
#include "hardware/LcdDisplay.h"
#include "state/FillingController.h"
#include "monitor/Monitor.h"
#include "utils/Timer.h"

#include <atomic>
#include <thread>

/**
 * @brief Top-level orchestrator class representing the AquaFlow application.
 *
 * Satisfies the strict architecture criteria that requires main.cpp to consist
 * purely of hardware initialisation and signal blocking.  AquaFlowApp owns:
 *  - The high-frequency tick timer (drives the FillingController state machine)
 *  - The Monitor observer (logs state transitions)
 *  - The keyboard input thread (simulates physical button presses over SSH)
 *
 * Keyboard controls (while connected via SSH):
 *   'b'  — short press: cycle cup size (Small → Medium → Large)
 *   's'  — long press:  confirm selected size
 *   Ctrl+C  — graceful shutdown via SIGINT
 *
 * When stdin is not a terminal (e.g. nohup / systemd), the keyboard thread
 * exits immediately and the application runs in proximity-only mode.
 */
class AquaFlowApp {
public:
    AquaFlowApp(IProximitySensor& proximitySensor,
                IPump&            pump,
                IFlowMeter&       flowMeter,
                LcdDisplay&       lcd);

    /** @brief Hooks up all callbacks and starts event-driven execution. */
    void start();

    /** @brief Signals all threads to stop and waits for clean exit. */
    void shutdown();

private:
    // Hardware abstractions (owned externally)
    IProximitySensor& gestureSensor_;
    IPump&            pump_;
    IFlowMeter&       flowMeter_;
    LcdDisplay&       lcd_;

    // Application components
    FillingController controller_;
    Monitor           monitor_;
    Timer             loopTimer_;

    // ── Keyboard simulation thread ────────────────────────────────────────────
    std::atomic<bool> running_{false};
    std::thread       keyboardThread_;

    /** Entry point for the keyboard input thread. */
    void runKeyboard();
};
