#include "hardware/FlowMeter.h"

#include <stdexcept>
#include <string>

// ── IHardwareDevice ───────────────────────────────────────────────────────────

bool FlowMeter::init() {
    if (running_) return true;  // Already started
    try {
        setupGpio();
    } catch (const std::exception& e) {
        return false;
    }
    running_ = true;
    pulseCount_ = 0;
    edgeThread_ = std::thread(&FlowMeter::edgeWorker, this);
    return true;
}

void FlowMeter::shutdown() {
    if (!running_) return;
    running_ = false;
    if (edgeThread_.joinable()) edgeThread_.join();
    // Release libgpiod resources (smart pointers handle cleanup)
    request_.reset();
    chip_.reset();
}

// ── Flow data API ─────────────────────────────────────────────────────────────

void FlowMeter::resetCount() {
    pulseCount_.store(0);
}

int FlowMeter::getPulseCount() const {
    return pulseCount_.load();
}

double FlowMeter::getVolumeML() const {
    return static_cast<double>(pulseCount_.load()) * mlPerPulse_;
}

bool FlowMeter::hasReachedTarget(double targetVolumeML) const {
    return getVolumeML() >= targetVolumeML;
}

// ── Private ───────────────────────────────────────────────────────────────────

void FlowMeter::setupGpio() {
    const std::string chipPath = "/dev/gpiochip" + std::to_string(chipNo_);
    chip_ = std::make_shared<gpiod::chip>(chipPath);

    gpiod::line_config lineCfg;
    lineCfg.add_line_settings(
        pinNo_,
        gpiod::line_settings()
            .set_direction(gpiod::line::direction::INPUT)
            // YF-S401 signal is open-collector: enable the Pi's internal 50kΩ
            // pull-up so the line idles HIGH and pulses LOW on each flow tick.
            // Without this the signal floats and produces spurious counts.
            .set_bias(gpiod::line::bias::PULL_UP)
            .set_edge_detection(gpiod::line::edge::FALLING));

    auto builder = chip_->prepare_request();
    builder.set_consumer("flow_meter");
    builder.set_line_config(lineCfg);
    request_ = std::make_shared<gpiod::line_request>(builder.do_request());
}

void FlowMeter::edgeWorker() {
    while (running_) {
        // Blocking wait — thread sleeps until a pulse arrives or 200 ms timeout
        if (request_->wait_edge_events(std::chrono::milliseconds(200))) {
            gpiod::edge_event_buffer buffer(8);
            std::size_t num = request_->read_edge_events(buffer);
            for (std::size_t i = 0; i < num; ++i) {
                if (buffer.get_event(i).type() ==
                    gpiod::edge_event::event_type::FALLING_EDGE) {
                    pulseCount_++;
                }
            }
        }
    }
}
