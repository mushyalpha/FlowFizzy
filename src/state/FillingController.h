#ifndef FILLINGCONTROLLER_H
#define FILLINGCONTROLLER_H

#include "hardware/GestureSensor.h"
#include "hardware/PumpController.h"
#include "hardware/FlowMeter.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <string>

/**
 * @brief System states for the filling cycle.
 *
 * WAITING       — No bottle detected, system idle
 * CONFIRMATION  — Proximity event received, waiting for stable hold
 * FILLING       — Pump ON, counting flow pulses
 * FILL_COMPLETE — Target volume reached, pump OFF, resetting
 */
enum class SystemState {
    WAITING,
    CONFIRMATION,
    FILLING,
    FILL_COMPLETE
};

/**
 * @brief Main state machine that orchestrates the filling cycle.
 *
 * Cup detection is now event-driven: GestureSensor fires a proximity
 * callback which sets a thread-safe flag.  tick() reads that flag on
 * each timer cycle — no polling, no distance measurements.
 *
 * The FillingController depends on abstractions (IHardwareDevice),
 * not on raw GPIO — supporting Dependency Inversion (SOLID "D").
 */
class FillingController {
public:
    /**
     * @param gestureSensor    Reference to the gesture/proximity sensor.
     * @param pump             Reference to the pump controller.
     * @param flowMeter        Reference to the flow meter.
     * @param holdTimeSeconds  How long proximity must be held to confirm a cup.
     * @param targetVolumeML   Target fill volume in millilitres.
     */
    FillingController(GestureSensor& gestureSensor,
                      PumpController& pump,
                      FlowMeter&      flowMeter,
                      int             holdTimeSeconds,
                      double          targetVolumeML);

    /**
     * @brief Run one cycle of the state machine.
     *
     * Call this repeatedly (e.g. from a Timer callback).
     * After each tick, calls the registered monitor callback (if any)
     * with the current state, volume, and bottle count.
     */
    void tick();

    /**
     * @brief Register a callback that is called after every tick.
     *
     * Used to wire in Monitor or any other observer without coupling
     * FillingController to a specific output class (SOLID OCP).
     */
    using MonitorCallback = std::function<void(const std::string& state,
                                               double volumeML,
                                               int    bottleCount)>;
    void registerMonitor(MonitorCallback cb) { monitorCallback_ = std::move(cb); }

    /** @brief Get the current system state. */
    SystemState getState() const;

    /** @brief Get a human-readable name for the current state. */
    std::string getStateName() const;

    /** @brief Get seconds elapsed in the hold timer (CONFIRMATION state). */
    double getHoldElapsed() const;

    /** @brief Get the current volume dispensed in ml. */
    double getCurrentVolumeML() const;

    /** @brief Get the target volume in ml. */
    double getTargetVolumeML() const;

    /** @brief Get the total number of bottles filled this session. */
    int getBottleCount() const;

#ifdef AQUAFLOW_TESTING
    /** @brief Test seam for unit tests to fast-forward confirmation timing. */
    void forceHoldElapsedForTest(double secondsElapsed);
#endif

private:
    // Hardware references
    GestureSensor& gestureSensor_;
    PumpController& pump_;
    FlowMeter& flowMeter_;

    // Settings
    int    holdTimeSeconds_;
    double targetVolumeML_;

    // State
    SystemState state_;
    std::chrono::steady_clock::time_point holdStartTime_;
    int bottleCount_;

    /**
     * @brief Thread-safe flag set by the GestureSensor proximity callback.
     *
     * true  = cup/bottle is currently detected in range
     * false = no cup detected
     *
     * Written from the GestureSensor worker thread, read from the Timer
     * callback thread — std::atomic ensures no data race.
     */
    std::atomic<bool> bottlePresent_{false};

    MonitorCallback monitorCallback_;  ///< Optional observer, called after each tick.
};

#endif // FILLINGCONTROLLER_H
