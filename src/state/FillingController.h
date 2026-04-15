#ifndef FILLINGCONTROLLER_H
#define FILLINGCONTROLLER_H

#include "IProximitySensor.h"
#include "IPump.h"
#include "IFlowMeter.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <string>

// ─── Cup size selection ───────────────────────────────────────────────────────

/**
 * @brief Three dispense sizes mapped to McDonald's UK soft-drink cup volumes.
 *
 * Small = 250 ml  |  Medium = 400 ml  |  Large = 500 ml
 */
enum class CupSize { SMALL, MEDIUM, LARGE };

// ─── System state machine ─────────────────────────────────────────────────────

/**
 * @brief Operational states of the AquaFlow filling cycle.
 *
 * SELECTING_SIZE  — User cycles S/M/L with short button presses;
 *                   a long press confirms the selection.
 * WAITING_FOR_CUP — Size confirmed; system waits for a cup within
 *                   proximity range (~5-10 cm from the APDS-9960).
 * CONFIRMING      — Cup detected; hold timer debounces the placement.
 * FILLING         — Pump ON; flow meter counts volume dispensed.
 * FILL_COMPLETE   — Target volume reached; pump OFF; waiting for cup removal.
 */
enum class SystemState {
    SELECTING_SIZE,
    WAITING_FOR_CUP,
    CONFIRMING,
    FILLING,
    FILL_COMPLETE
};

/**
 * @brief Main state machine that orchestrates the AquaFlow filling cycle.
 *
 * User interaction model:
 *  - Short button press  → cycle cup size (S → M → L → S).
 *  - Long button press   → confirm selected size; enter WAITING_FOR_CUP.
 *  - Cup placed in range → CONFIRMING hold timer starts.
 *  - Cup held steady     → pump starts; FILLING begins.
 *  - Target volume reached → pump stops; FILL_COMPLETE.
 *  - Cup removed         → return to SELECTING_SIZE.
 *
 * Cross-thread safety:
 *  Button events (onShortPress / onLongPress) are called from a keyboard or
 *  GPIO thread.  Proximity events are posted from the sensor worker thread.
 *  All shared state uses std::atomic to prevent data races.
 *
 * The class depends only on behaviour abstractions (IProximitySensor,
 * IPump, IFlowMeter), preserving SOLID OCP and DIP.
 */
class FillingController {
public:
    FillingController(IProximitySensor& proximitySensor,
                      IPump&            pump,
                      IFlowMeter&       flowMeter);

    /** @brief Run one cycle of the state machine (call from timer callback). */
    void tick();

    // ── Button event handlers ─────────────────────────────────────────────────

    /**
     * @brief Short button press — cycle Small → Medium → Large → Small.
     * No-op unless in SELECTING_SIZE.  Thread-safe.
     */
    void onShortPress();

    /**
     * @brief Long button press — confirm the currently selected size.
     * No-op unless in SELECTING_SIZE.  Thread-safe.
     */
    void onLongPress();

    // ── Observer ──────────────────────────────────────────────────────────────

    using MonitorCallback = std::function<void(const std::string& state,
                                               double volumeML,
                                               int    bottleCount)>;
    void registerMonitor(MonitorCallback cb) { monitorCallback_ = std::move(cb); }

    // ── Accessors ─────────────────────────────────────────────────────────────

    SystemState getState()           const;
    std::string getStateName()       const;  ///< e.g. "SELECT:MEDIUM", "FILLING"
    std::string getSizeName()        const;  ///< "SMALL" | "MEDIUM" | "LARGE"
    double      getTargetVolumeML()  const;
    double      getCurrentVolumeML() const;
    int         getBottleCount()     const;

private:
    IProximitySensor& proximitySensor_;
    IPump&            pump_;
    IFlowMeter&       flowMeter_;

    // ── Size selection (written by button thread, read by timer thread) ───────
    std::atomic<CupSize> selectedSize_{CupSize::SMALL};
    std::atomic<bool>    sizeConfirmed_{false};   ///< set by onLongPress, consumed by tick

    // ── Fill target (set once on size confirmation) ───────────────────────────
    std::atomic<double>  targetVolumeML_{0.0};

    // ── State-machine internals ───────────────────────────────────────────────
    SystemState state_{SystemState::SELECTING_SIZE};
    std::chrono::steady_clock::time_point holdStartTime_;
    int bottleCount_{0};

    /**
     * @brief Thread-safe cup-presence flag.
     *
     * Written (release) by the IProximitySensor callback thread;
     * read (acquire) by tick() on the Timer thread.
     */
    std::atomic<bool> bottlePresent_{false};

    MonitorCallback monitorCallback_;
};

#endif // FILLINGCONTROLLER_H
