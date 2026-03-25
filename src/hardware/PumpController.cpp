#include "hardware/PumpController.h"

#include <stdexcept>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────

PumpController::PumpController(unsigned int chipNo, unsigned int pumpPin)
    : chipNo_(chipNo), pumpPin_(pumpPin) {}

PumpController::~PumpController() {
    shutdown();
}

// ── IHardwareDevice ───────────────────────────────────────────────────────────

bool PumpController::init() {
    if (initialised_) return true;

    try {
        const std::string chipPath = "/dev/gpiochip" + std::to_string(chipNo_);
        chip_ = std::make_shared<gpiod::chip>(chipPath);

        // Configure the pump pin as output, idle = pump OFF
        gpiod::line_config lineCfg;
        lineCfg.add_line_settings(
            pumpPin_,
            gpiod::line_settings()
                .set_direction(gpiod::line::direction::OUTPUT)
                .set_output_value(offValue()));   // start de-energised

        auto builder = chip_->prepare_request();
        builder.set_consumer("pump_controller");
        builder.set_line_config(lineCfg);
        request_ = std::make_shared<gpiod::line_request>(builder.do_request());

        running_     = false;
        initialised_ = true;

        std::cout << "PumpController initialised via libgpiod (chip="
                  << chipNo_ << ", pin=" << pumpPin_
                  << ", mode=" << (RELAY_MODE ? "RELAY(active-LOW)" : "TRANSISTOR(active-HIGH)")
                  << ")\n";
        return true;

    } catch (const std::exception& e) {
        std::cerr << "PumpController::init() error: " << e.what() << "\n";
        return false;
    }
}

void PumpController::shutdown() {
    if (!initialised_) return;
    // Ensure pump is off before releasing GPIO
    request_->set_value(pumpPin_, offValue());
    running_     = false;
    initialised_ = false;
    request_.reset();
    chip_.reset();
    std::cout << "PumpController shut down (pump OFF).\n";
}

// ── Pump control ──────────────────────────────────────────────────────────────

void PumpController::turnOn() {
    if (!initialised_ || running_) return;
    request_->set_value(pumpPin_, onValue());
    running_ = true;
    std::cout << "Pump started!\n";
}

void PumpController::turnOff() {
    if (!initialised_ || !running_) return;
    request_->set_value(pumpPin_, offValue());
    running_ = false;
    std::cout << "Pump stopped.\n";
}

bool PumpController::isRunning() const { return running_; }

// ── Helpers ───────────────────────────────────────────────────────────────────

gpiod::line::value PumpController::onValue() const {
    // Relay (active-LOW): energise coil by pulling LOW
    // Transistor (active-HIGH): switch ON by pulling HIGH
    return RELAY_MODE ? gpiod::line::value::INACTIVE
                      : gpiod::line::value::ACTIVE;
}

gpiod::line::value PumpController::offValue() const {
    return RELAY_MODE ? gpiod::line::value::ACTIVE
                      : gpiod::line::value::INACTIVE;
}
