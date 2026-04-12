#include "state/FillingController.h"

#include <iostream>
#include <iomanip>

using Clock = std::chrono::steady_clock;

// ─────────────────────────────────────────────────────────────────────────────

FillingController::FillingController(GestureSensor& gestureSensor,
                                     PumpController& pump,
                                     FlowMeter&      flowMeter,
                                     int             holdTimeSeconds,
                                     double          targetVolumeML)
    : gestureSensor_(gestureSensor),
      pump_(pump),
      flowMeter_(flowMeter),
      holdTimeSeconds_(holdTimeSeconds),
      targetVolumeML_(targetVolumeML),
      state_(SystemState::WAITING),
      holdStartTime_(),
      bottleCount_(0),
      monitorCallback_(nullptr)
{
    // Register proximity callback on the GestureSensor.
    // This lambda is called from the GestureSensor worker thread whenever
    // proximity state changes.  It only writes to bottlePresent_ (atomic),
    // so no mutex needed — the actual state-machine logic stays in tick().
    gestureSensor_.registerEventCallback([this](const GestureEvent& event) {
        if (event.state == ProximityState::PROXIMITY_TRIGGERED) {
            bottlePresent_.store(true, std::memory_order_relaxed);
            std::cout << "[GestureSensor] Proximity TRIGGERED (val="
                      << event.proximityValue << ")\n";
        } else if (event.state == ProximityState::PROXIMITY_CLEARED) {
            bottlePresent_.store(false, std::memory_order_relaxed);
            std::cout << "[GestureSensor] Proximity CLEARED\n";
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────

void FillingController::tick() {
    std::cout << std::fixed << std::setprecision(2);

    switch (state_) {

    // ── WAITING: no cup detected yet ─────────────────────────────────────────
    case SystemState::WAITING: {
        if (bottlePresent_.load(std::memory_order_relaxed)) {
            // Proximity event fired — start hold timer
            holdStartTime_ = Clock::now();
            state_ = SystemState::CONFIRMATION;
            std::cout << "Cup detected! Starting confirmation timer...\n";
        }
        break;
    }

    // ── CONFIRMATION: cup must stay present for holdTimeSeconds_ ─────────────
    case SystemState::CONFIRMATION: {
        if (!bottlePresent_.load(std::memory_order_relaxed)) {
            // Cup removed before timer expired — reset
            std::cout << "Cup removed before confirmation — resetting.\n";
            state_ = SystemState::WAITING;
            break;
        }

        double elapsed = getHoldElapsed();
        std::cout << std::setprecision(1)
                  << "Confirming cup: " << elapsed
                  << " / " << holdTimeSeconds_ << " s\n";

        if (elapsed >= holdTimeSeconds_) {
            // Confirmed — reset meter and start pump
            flowMeter_.resetCount();
            pump_.turnOn();
            state_ = SystemState::FILLING;
            std::cout << "Cup confirmed! Filling to "
                      << std::setprecision(0) << targetVolumeML_ << " ml\n";
        }
        break;
    }

    // ── FILLING: pump running, counting flow pulses ───────────────────────────
    case SystemState::FILLING: {
        double currentML = flowMeter_.getVolumeML();
        int    pulses    = flowMeter_.getPulseCount();

        std::cout << std::setprecision(1)
                  << "Filling: " << currentML << " ml / "
                  << targetVolumeML_ << " ml"
                  << "  (" << pulses << " pulses)\n";

        if (flowMeter_.hasReachedTarget(targetVolumeML_)) {
            pump_.turnOff();
            bottleCount_++;
            state_ = SystemState::FILL_COMPLETE;
            std::cout << std::setprecision(1)
                      << "Target reached! Dispensed " << currentML << " ml. "
                      << "Bottles filled: " << bottleCount_ << "\n";
        }
        break;
    }

    // ── FILL_COMPLETE: pump off, reset for next cup ───────────────────────────
    case SystemState::FILL_COMPLETE: {
        flowMeter_.resetCount();
        state_ = SystemState::WAITING;
        std::cout << "Fill complete. Waiting for next cup...\n";
        break;
    }
    }

    // Notify any registered observer after every tick
    if (monitorCallback_) {
        monitorCallback_(getStateName(), flowMeter_.getVolumeML(), bottleCount_);
    }
}

// ─────────────────────────────────────────────────────────────────────────────

SystemState FillingController::getState() const {
    return state_;
}

std::string FillingController::getStateName() const {
    switch (state_) {
        case SystemState::WAITING:       return "WAITING";
        case SystemState::CONFIRMATION:  return "CONFIRMATION";
        case SystemState::FILLING:       return "FILLING";
        case SystemState::FILL_COMPLETE: return "FILL_COMPLETE";
        default:                         return "UNKNOWN";
    }
}

double FillingController::getHoldElapsed() const {
    if (state_ != SystemState::CONFIRMATION && state_ != SystemState::FILLING) {
        return 0.0;
    }
    auto now = Clock::now();
    return std::chrono::duration<double>(now - holdStartTime_).count();
}

double FillingController::getCurrentVolumeML() const {
    return flowMeter_.getVolumeML();
}

double FillingController::getTargetVolumeML() const {
    return targetVolumeML_;
}

int FillingController::getBottleCount() const {
    return bottleCount_;
}
