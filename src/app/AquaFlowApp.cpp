#include "app/AquaFlowApp.h"
#include "PinConfig.h"
#include "utils/Logger.h"

#include <iomanip>
#include <sstream>
#include <thread>

// POSIX terminal / read
#include <unistd.h>
#include <termios.h>

// ─────────────────────────────────────────────────────────────────────────────

AquaFlowApp::AquaFlowApp(IProximitySensor& proximitySensor,
                         IPump&            pump,
                         IFlowMeter&       flowMeter,
                         LcdDisplay&       lcd)
    : gestureSensor_(proximitySensor),
      pump_(pump),
      flowMeter_(flowMeter),
      lcd_(lcd),
      controller_(proximitySensor, pump, flowMeter),
      loopTimer_(LOOP_INTERVAL_MS)
{}

// ─────────────────────────────────────────────────────────────────────────────

void AquaFlowApp::start() {

    // 1. Wire state transitions to the monitor and the LCD display
    controller_.registerMonitor([this](const std::string& state,
                                       double             vol,
                                       int                cups)
    {
        monitor_.onStateChange(state, vol, cups);

        // ── LCD row 0: state label ──────────────────────────────────────────
        if (state.rfind("SELECT:", 0) == 0) {
            // e.g. "SELECT:MEDIUM"
            lcd_.print(0, "Size: " + state.substr(7));                   // "Size: MEDIUM"
            lcd_.print(1, "'b'cycle  's'select");

        } else if (state == "PLACE CUP") {
            lcd_.print(0, "Place cup...");
            std::ostringstream r1;
            r1 << "Target: " << static_cast<int>(controller_.getTargetVolumeML()) << " ml";
            lcd_.print(1, r1.str());

        } else if (state == "CONFIRMING") {
            lcd_.print(0, "Hold steady...");
            lcd_.print(1, "Confirming cup");

        } else if (state == "FILLING") {
            lcd_.print(0, "Filling...");
            // Volume display updated at high frequency in the timer callback below

        } else if (state == "COMPLETE") {
            lcd_.print(0, "Done!  Remove cup");
            std::ostringstream r1;
            r1 << std::fixed << std::setprecision(0)
               << vol << " ml  (cup " << cups << ")";
            lcd_.print(1, r1.str());
        }
    });

    // 2. Wire the high-frequency state-machine tick loop
    loopTimer_.registerCallback([this]() {
        controller_.tick();

        // Live volume update during filling — bypasses the lower-frequency monitor
        if (controller_.getState() == SystemState::FILLING) {
            std::ostringstream row;
            row << std::fixed << std::setprecision(0)
                << flowMeter_.getVolumeML() << " / "
                << static_cast<int>(controller_.getTargetVolumeML()) << " ml";
            lcd_.print(1, row.str());
        }
    });

    // 3. Start the keyboard simulation thread
    running_.store(true, std::memory_order_relaxed);
    keyboardThread_ = std::thread(&AquaFlowApp::runKeyboard, this);

    // 4. Start the tick timer
    loopTimer_.start();
}

// ─────────────────────────────────────────────────────────────────────────────

void AquaFlowApp::shutdown() {
    loopTimer_.stop();

    running_.store(false, std::memory_order_relaxed);
    if (keyboardThread_.joinable()) keyboardThread_.join();

    lcd_.clear();
    lcd_.print(0, "AquaFlow");
    lcd_.print(1, "Shutting down...");
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
}

// ─────────────────────────────────────────────────────────────────────────────

void AquaFlowApp::runKeyboard() {
    // Skip when stdin is not connected to a real terminal (nohup, systemd, pipe)
    if (!isatty(STDIN_FILENO)) {
        Logger::info("[Input] No terminal — keyboard simulation disabled. "
                     "Attach a physical button to GPIO " + std::to_string(BUTTON_PIN) + ".");
        return;
    }

    // Switch stdin to raw, non-echo, non-blocking single-character mode
    struct termios oldt{}, newt{};
    tcgetattr(STDIN_FILENO, &oldt);
    newt          = oldt;
    newt.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);  // raw mode; keep ISIG (Ctrl+C works)
    newt.c_cc[VMIN]  = 0;   // non-blocking read
    newt.c_cc[VTIME] = 1;   // 100 ms timeout — allows running_.load() check
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    Logger::info("[Input] Keyboard ready:   'b' = cycle size   's' / Enter = select   Ctrl+C = quit");

    while (running_.load(std::memory_order_relaxed)) {
        char c = 0;
        if (::read(STDIN_FILENO, &c, 1) != 1) continue;   // timeout or error

        switch (c) {
            case 'b': case 'B':
                controller_.onShortPress();
                break;
            case 's': case 'S': case '\r': case '\n':
                controller_.onLongPress();
                break;
            default:
                break;
        }
    }

    // Restore terminal to its original settings on exit
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}
