#include "PinConfig.h"
#include "hardware/PumpController.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

// ─── Graceful shutdown on Ctrl+C ─────────────────────────────────────────────
static std::atomic<bool> keepRunning(true);

void signalHandler(int signum) {
    std::cout << "\n[Signal " << signum << "] Stopping...\n";
    keepRunning = false;
}

int main() {
    std::signal(SIGINT, signalHandler);

    std::cout << "=== Pump Integration Test ===\n";
    std::cout << "Using libgpiod on Chip " << GPIO_CHIP_NO << ", Pin " << PUMP_PIN << "\n";

    // Initialise RTES-compliant pump controller (uses libgpiod)
    PumpController pump(GPIO_CHIP_NO, PUMP_PIN);

    if (!pump.init()) {
        std::cerr << "Failed to initialise pump hardware.\n";
        return 1;
    }

    std::cout << "Starting 3-cycle test...\n";

    for (int i = 0; i < 3 && keepRunning; ++i) {
        std::cout << "Cycle " << i + 1 << ": PUMP ON\n";
        pump.turnOn();
        // Since this is just a sequential integration test script, sleep_for is acceptable here
        // (the main application uses event-driven state machines, not sleeps)
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (!keepRunning) break;

        std::cout << "Cycle " << i + 1 << ": PUMP OFF\n";
        pump.turnOff();
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    // Ensure it's off before exit
    pump.shutdown();
    std::cout << "Test complete. Pump OFF.\n";

    return 0;
}
