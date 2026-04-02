#include "PinConfig.h"
#include "hardware/PumpController.h"
#include "hardware/FlowMeter.h"
#include "hardware/LcdDisplay.h"

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

    std::cout << "=== Pump + Flow Meter + LCD Integration Test ===\n";
    std::cout << "Target volume : " << TARGET_VOLUME_ML << " ml\n";
    std::cout << "ML per pulse  : " << ML_PER_PULSE << "\n\n";

    // ── Construct ─────────────────────────────────────────────────────────────
    PumpController pump(GPIO_CHIP_NO, PUMP_PIN);
    FlowMeter      flow(GPIO_CHIP_NO, FLOW_PIN, static_cast<float>(ML_PER_PULSE));
    LcdDisplay     lcd(LCD_I2C_BUS, LCD_I2C_ADDRESS);

    // ── Initialise ────────────────────────────────────────────────────────────
    if (!pump.init()) {
        std::cerr << "ERROR: PumpController failed to init\n"; return 1;
    }
    if (!flow.init()) {
        std::cerr << "ERROR: FlowMeter failed to init\n";
        pump.shutdown(); return 1;
    }
    if (!lcd.init()) {
        // Non-fatal — continue without display
        std::cerr << "WARNING: LcdDisplay failed to init — continuing without display\n";
    }

    // ── Splash ────────────────────────────────────────────────────────────────
    lcd.print(0, "AquaFlow Test");
    lcd.print(1, "Starting...");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    if (!keepRunning) goto cleanup;

    // ── Reset flow counter and start pump ─────────────────────────────────────
    flow.resetCount();
    lcd.showStatus("FILLING", 0.0, 0);
    pump.turnOn();
    std::cout << "Pump ON — dispensing to " << TARGET_VOLUME_ML << " ml...\n";

    // ── Live volume loop ──────────────────────────────────────────────────────
    // (sleep_for is acceptable here — this is a sequential integration test,
    //  not the event-driven main application)
    while (keepRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        double vol = flow.getVolumeML();
        int    pulses = flow.getPulseCount();

        // Update LCD row 1 with live volume
        lcd.showVolume(vol);

        // Console readout
        std::cout << "\r  Pulses: " << std::setw(5) << pulses
                  << "   Vol: " << std::fixed << std::setprecision(1)
                  << std::setw(7) << vol << " ml"
                  << std::flush;

        // Stop when target reached
        if (flow.hasReachedTarget(TARGET_VOLUME_ML)) {
            std::cout << "\n\nTarget reached!\n";
            break;
        }
    }

    // ── Stop pump ─────────────────────────────────────────────────────────────
    pump.turnOff();
    {
        double finalVol = flow.getVolumeML();
        std::cout << "Final volume : " << std::fixed << std::setprecision(2)
                  << finalVol << " ml (" << flow.getPulseCount() << " pulses)\n";

        lcd.showStatus("DONE", finalVol, 1);
        lcd.showVolume(finalVol);
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

cleanup:
    pump.shutdown();
    flow.shutdown();
    lcd.shutdown();

    std::cout << "Test complete.\n";
    return 0;
}
