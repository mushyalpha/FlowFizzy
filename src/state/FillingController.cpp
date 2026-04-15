#include "state/FillingController.h"
#include "PinConfig.h"
#include "utils/Logger.h"

#include <chrono>
#include <sstream>

using Clock = std::chrono::steady_clock;

// ─────────────────────────────────────────────────────────────────────────────

FillingController::FillingController(IProximitySensor& proximitySensor,
                                     IPump&            pump,
                                     IFlowMeter&       flowMeter)
    : proximitySensor_(proximitySensor),
      pump_(pump),
      flowMeter_(flowMeter)
{
    // Register proximity callback — called from the sensor worker thread.
    // Intentionally minimal: atomic store only — no I/O, no blocking.
    // memory_order_release pairs with the acquire-load in tick() to ensure
    // the stored value is visible to the Timer thread immediately.
    proximitySensor_.registerEventCallback([this](const GestureEvent& event) {
        if (event.getState() == ProximityState::PROXIMITY_TRIGGERED) {
            bottlePresent_.store(true,  std::memory_order_release);
        } else if (event.getState() == ProximityState::PROXIMITY_CLEARED) {
            bottlePresent_.store(false, std::memory_order_release);
        }
        // Gesture directions are deliberately ignored in this design.
        // Proximity-only detection proved reliable; gesture detection did not.
    });
}

// ─── Button event handlers ────────────────────────────────────────────────────

void FillingController::onShortPress() {
    if (state_ != SystemState::SELECTING_SIZE) return;

    // Cycle: SMALL → MEDIUM → LARGE → SMALL
    CupSize cur = selectedSize_.load(std::memory_order_relaxed);
    switch (cur) {
        case CupSize::SMALL:  selectedSize_.store(CupSize::MEDIUM, std::memory_order_relaxed); break;
        case CupSize::MEDIUM: selectedSize_.store(CupSize::LARGE,  std::memory_order_relaxed); break;
        case CupSize::LARGE:  selectedSize_.store(CupSize::SMALL,  std::memory_order_relaxed); break;
    }
    Logger::info("Size: " + getSizeName());
}

void FillingController::onLongPress() {
    if (state_ != SystemState::SELECTING_SIZE) return;
    sizeConfirmed_.store(true, std::memory_order_release);
}

// ─── Main state machine tick ─────────────────────────────────────────────────

void FillingController::tick() {

    switch (state_) {

    // ── SELECTING_SIZE: cycle S/M/L with short press; long press to confirm ──
    case SystemState::SELECTING_SIZE: {
        if (sizeConfirmed_.exchange(false, std::memory_order_acq_rel)) {
            switch (selectedSize_.load(std::memory_order_relaxed)) {
                case CupSize::SMALL:  targetVolumeML_.store(CUP_SMALL_ML);  break;
                case CupSize::MEDIUM: targetVolumeML_.store(CUP_MEDIUM_ML); break;
                case CupSize::LARGE:  targetVolumeML_.store(CUP_LARGE_ML);  break;
            }
            std::ostringstream msg;
            msg << "Size confirmed: " << getSizeName()
                << " (" << static_cast<int>(targetVolumeML_.load()) << " ml)"
                << " — place your cup within range.";
            Logger::info(msg.str());
            state_ = SystemState::WAITING_FOR_CUP;
        }
        break;
    }

    // ── WAITING_FOR_CUP: proximity cross-over triggers confirmation hold ──────
    case SystemState::WAITING_FOR_CUP: {
        if (bottlePresent_.load(std::memory_order_acquire)) {
            holdStartTime_ = Clock::now();
            state_ = SystemState::CONFIRMING;
        }
        break;
    }

    // ── CONFIRMING: cup must remain in range for CUP_CONFIRM_MS ms ───────────
    case SystemState::CONFIRMING: {
        if (!bottlePresent_.load(std::memory_order_acquire)) {
            // Cup wobbled or removed — go back and wait again
            state_ = SystemState::WAITING_FOR_CUP;
            break;
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - holdStartTime_).count();
        if (elapsed >= CUP_CONFIRM_MS) {
            flowMeter_.resetCount();
            pump_.turnOn();
            Logger::info("Pump started!");
            state_ = SystemState::FILLING;
        }
        break;
    }

    // ── FILLING: pump running, counting flow pulses ───────────────────────────
    case SystemState::FILLING: {
        if (!bottlePresent_.load(std::memory_order_acquire)) {
            // Emergency abort — cup removed during fill
            pump_.turnOff();
            Logger::info("Cup removed — fill aborted. Place cup to retry.");
            state_ = SystemState::WAITING_FOR_CUP;
            break;
        }
        if (flowMeter_.getVolumeML() >= targetVolumeML_.load(std::memory_order_relaxed)) {
            pump_.turnOff();
            bottleCount_++;
            state_ = SystemState::FILL_COMPLETE;
        }
        break;
    }

    // ── FILL_COMPLETE: pump off; wait for cup removal to reset ───────────────
    case SystemState::FILL_COMPLETE: {
        if (!bottlePresent_.load(std::memory_order_acquire)) {
            flowMeter_.resetCount();
            // Return to size selection so the user can pick a new size
            // (or just press 's' again to refill the same size)
            state_ = SystemState::SELECTING_SIZE;
        }
        break;
    }

    } // end switch

    if (monitorCallback_) {
        monitorCallback_(getStateName(), flowMeter_.getVolumeML(), bottleCount_);
    }
}

// ─── Accessors ────────────────────────────────────────────────────────────────

SystemState FillingController::getState() const { return state_; }

std::string FillingController::getSizeName() const {
    switch (selectedSize_.load(std::memory_order_relaxed)) {
        case CupSize::SMALL:  return "SMALL";
        case CupSize::MEDIUM: return "MEDIUM";
        case CupSize::LARGE:  return "LARGE";
        default:              return "UNKNOWN";
    }
}

std::string FillingController::getStateName() const {
    switch (state_) {
        case SystemState::SELECTING_SIZE:  return "SELECT:" + getSizeName();
        case SystemState::WAITING_FOR_CUP: return "PLACE CUP";
        case SystemState::CONFIRMING:      return "CONFIRMING";
        case SystemState::FILLING:         return "FILLING";
        case SystemState::FILL_COMPLETE:   return "COMPLETE";
        default:                           return "UNKNOWN";
    }
}

double FillingController::getTargetVolumeML() const {
    return targetVolumeML_.load(std::memory_order_relaxed);
}

double FillingController::getCurrentVolumeML() const {
    return flowMeter_.getVolumeML();
}

int FillingController::getBottleCount() const { return bottleCount_; }
