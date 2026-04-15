#include "monitor/Monitor.h"
#include "utils/Logger.h"

#include <sstream>
#include <iomanip>

void Monitor::onStateChange(const std::string& state,
                             double volumeML,
                             int    bottleCount) {
    // ── Log meaningful transitions ────────────────────────────────────────────
    if (state != lastState_) {
        if (!lastState_.empty()) {
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
        }
        lastState_ = state;
    }

    // ── Periodic status line (every tick) ────────────────────────────────────
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1)
        << "[State: " << state
        << " | Volume: " << volumeML << " ml"
        << " | Cups: "  << bottleCount << "]";
    Logger::info(oss.str());
}
