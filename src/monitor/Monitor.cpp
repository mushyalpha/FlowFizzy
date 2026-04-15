#include "monitor/Monitor.h"
#include "utils/Logger.h"

#include <sstream>
#include <iomanip>
#include <cmath>

void Monitor::onStateChange(const std::string& state,
                             double volumeML,
                             int    bottleCount) {

    const bool stateChanged  = (state != lastState_);
    // Only treat volume as "changed" when actively filling (avoids idle noise)
    const bool volumeChanged = (state == "FILLING") &&
                               (std::abs(volumeML - lastVolumeML_) >= 10.0);

    // ── Log meaningful state transition messages ──────────────────────────────
    if (stateChanged && !lastState_.empty()) {
        if (state == "CONFIRMING") {
            Logger::info("Cup detected! Confirming placement — hold steady...");
        } else if (state == "PLACE CUP") {
            if (lastState_ == "CONFIRMING") {
                Logger::info("Cup removed before confirmation — please hold the cup steady.");
            } else if (lastState_ == "COMPLETE") {
                Logger::info("Cup removed. Ready for next fill.");
            }
        } else if (state == "FILLING") {
            Logger::info("Cup confirmed! Filling in progress...");
        } else if (state == "COMPLETE") {
            std::ostringstream oss;
            oss << "Target reached! Dispensed "
                << std::fixed << std::setprecision(1) << volumeML
                << " ml. Cups filled this session: " << bottleCount;
            Logger::info(oss.str());
        } else if (state.rfind("SELECT:", 0) == 0 && lastState_ == "COMPLETE") {
            Logger::info("Fill complete. Press 'b' to cycle size or 's' to refill.");
        }
        lastState_ = state;
    }

    // ── Status line: only print when something actually changed ───────────────
    if (stateChanged || volumeChanged) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1)
            << "[State: " << state
            << " | Volume: " << volumeML << " ml"
            << " | Cups: "  << bottleCount << "]";
        Logger::info(oss.str());
        lastVolumeML_ = volumeML;
    }
}
