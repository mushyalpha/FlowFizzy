#pragma once

#include "IHardwareDevice.h"

#include <memory>
#include <string>

#include <gpiod.hpp>

/**
 * @brief DC Water Pump controller via relay module or TIP121 transistor.
 *
 * Uses libgpiod v2 (the officially supported GPIO library for Linux)
 * to drive a single output pin HIGH/LOW.  This replaces the previous
 * lgpio-based implementation which used a hobbyist library not officially
 * supported on Raspberry Pi OS.
 *
 * Relay mode (active-LOW): set RELAY_MODE = true  → pin LOW  = pump ON
 * Transistor mode:         set RELAY_MODE = false → pin HIGH = pump ON
 */
class PumpController : public IHardwareDevice {
public:
    /**
     * @param chipNo   GPIO chip number (e.g. 4 for /dev/gpiochip4 on Pi 5).
     * @param pumpPin  BCM pin number for pump control output.
     */
    PumpController(unsigned int chipNo, unsigned int pumpPin);
    ~PumpController() override;

    bool init() override;
    void shutdown() override;

    /** @brief Turn the pump ON. */
    void turnOn();

    /** @brief Turn the pump OFF. */
    void turnOff();

    /** @brief True while the pump is running. */
    bool isRunning() const;

private:
    // Relay module: active-LOW (de-energise = HIGH, pump ON = LOW)
    // Transistor:   active-HIGH (pin HIGH = pump ON)
    static constexpr bool RELAY_MODE = true;

    gpiod::line::value onValue()  const;
    gpiod::line::value offValue() const;

    unsigned int chipNo_;
    unsigned int pumpPin_;
    bool running_    = false;
    bool initialised_= false;

    std::shared_ptr<gpiod::chip>         chip_;
    std::shared_ptr<gpiod::line_request> request_;
};
