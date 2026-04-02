#include "PinConfig.h"
#include "hardware/LcdDisplay.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

// ─── Graceful shutdown on Ctrl+C ─────────────────────────────────────────────
static std::atomic<bool> keepRunning(true);

void signalHandler(int signum) {
    std::cout << "\n[Signal " << signum << "] Stopping...\n";
    keepRunning = false;
}

int main() {
    std::signal(SIGINT, signalHandler);

    std::cout << "=== LCD Integration Test ===\n";
    std::cout << "Bus /dev/i2c-" << LCD_I2C_BUS
              << ", Address 0x" << std::hex << LCD_I2C_ADDRESS << std::dec << "\n";

    LcdDisplay lcd(LCD_I2C_BUS, LCD_I2C_ADDRESS);

    if (!lcd.init()) {
        std::cerr << "Failed to initialise LCD. Check:\n"
                  << "  1. i2c is enabled: sudo raspi-config → Interface Options → I2C\n"
                  << "  2. Address:        i2cdetect -y 1\n"
                  << "  3. Wiring:         VDD→Pin2(5V) VSS→Pin6(GND) SDA→Pin3 SCL→Pin5\n";
        return 1;
    }

    std::cout << "LCD initialised. Running display cycle...\n";

    // ── Test sequence — mirrors what the real system displays ─────────────────

    // 1. Splash screen
    lcd.print(0, "   AquaFlow    ");
    lcd.print(1, "  Water Disp.  ");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    if (!keepRunning) goto cleanup;

    // 2. IDLE state
    lcd.showStatus("IDLE", 0.0, 0);
    std::cout << "Showing: IDLE / Bottles: 0\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    if (!keepRunning) goto cleanup;

    // 3. FILLING — simulate real-time volume ticking up (as flow meter would)
    lcd.showStatus("FILLING", 0.0, 1);
    std::cout << "Simulating real-time volume fill...\n";
    {
        double vol = 0.0;
        while (keepRunning && vol <= 500.0) {
            lcd.showVolume(vol);

            std::ostringstream oss;
            oss << "Vol: " << std::fixed << std::setprecision(1) << vol << " ml";
            std::cout << "\r  " << oss.str() << std::flush;

            vol += 25.0;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        std::cout << "\n";
    }
    if (!keepRunning) goto cleanup;

    // 4. DONE state
    lcd.showStatus("DONE", 500.0, 1);
    std::cout << "Showing: DONE / Bottles: 1\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    if (!keepRunning) goto cleanup;

    // 5. Back to IDLE, bottle count incremented
    lcd.showStatus("IDLE", 0.0, 1);
    std::cout << "Showing: IDLE / Bottles: 1\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

cleanup:
    lcd.shutdown();
    std::cout << "Test complete.\n";
    return 0;
}
