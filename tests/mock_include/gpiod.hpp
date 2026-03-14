/**
 * @file gpiod.hpp (MOCK)
 * @brief Mock libgpiod v2 C++ header for Windows/WSL testing without GPIO hardware.
 *
 * Simulates:
 *   - FlowSensor: falling-edge events at ~10 Hz
 *   - UltrasonicSensor: rising+falling edge pairs with ~700µs gap (~12 cm)
 */

#ifndef GPIOD_HPP_MOCK
#define GPIOD_HPP_MOCK

#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <atomic>
#include <mutex>

namespace gpiod {

// ─── Forward declarations ────────────────────────────────────────────────────

namespace line {
    enum class direction { INPUT, OUTPUT };
    enum class edge      { RISING, FALLING, BOTH };
    enum class value     { INACTIVE = 0, ACTIVE = 1 };
}

// ─── edge_event ──────────────────────────────────────────────────────────────
// Base class holds the event_type struct so edge_event::event_type::RISING_EDGE
// resolves through inheritance, while ev.event_type() resolves to the method.

struct edge_event_types {
    struct event_type {
        static constexpr int RISING_EDGE  = 0;
        static constexpr int FALLING_EDGE = 1;
    };
};

class edge_event : public edge_event_types {
public:
    edge_event() : type_val_(event_type::FALLING_EDGE), ts_ns_(0) {}
    edge_event(int t, uint64_t ts) : type_val_(t), ts_ns_(ts) {}

    int event_type() const { return type_val_; }
    uint64_t timestamp_ns() const { return ts_ns_; }

private:
    int type_val_;
    uint64_t ts_ns_;
};

// ─── edge_event_buffer ───────────────────────────────────────────────────────

class edge_event_buffer {
public:
    explicit edge_event_buffer(std::size_t capacity)
        : capacity_(capacity) {}

    const edge_event& get_event(std::size_t idx) const { return events_[idx]; }
    std::size_t size() const { return events_.size(); }

    void push_event(const edge_event& ev) { events_.push_back(ev); }
    void clear() { events_.clear(); }

private:
    std::size_t capacity_;
    std::vector<edge_event> events_;
    friend class line_request;
};

// ─── line_settings ───────────────────────────────────────────────────────────

class line_settings {
public:
    line_settings& set_direction(line::direction d) { dir_ = d; return *this; }
    line_settings& set_edge_detection(line::edge e) { edge_ = e; return *this; }
    line_settings& set_output_value(line::value v) { return *this; }

    line::direction dir_ = line::direction::INPUT;
    line::edge edge_ = line::edge::FALLING;
};

// ─── line_config ─────────────────────────────────────────────────────────────

class line_config {
public:
    void add_line_settings(std::initializer_list<unsigned int> pins, line_settings s) {
        settings_ = s;
    }
    line_settings settings_;
};

// ─── line_request ────────────────────────────────────────────────────────────

class line_request {
public:
    enum class request_mode { FLOW_INPUT, ECHO_INPUT, TRIG_OUTPUT };

    line_request() : mode_(request_mode::FLOW_INPUT) {}
    explicit line_request(request_mode m) : mode_(m) {}

    // set_value — for trigger output
    void set_value(unsigned int pin, line::value val) {
        if (mode_ == request_mode::TRIG_OUTPUT && val == line::value::ACTIVE) {
            // Trigger went HIGH — schedule simulated echo edges
            trigFired_ = true;
            trigTime_ = now_ns();
        }
    }

    // get_value — for input pins
    line::value get_value(unsigned int pin) {
        return line::value::INACTIVE;
    }

    // wait_edge_events — simulates edge events
    bool wait_edge_events(std::chrono::milliseconds timeout) {
        if (mode_ == request_mode::ECHO_INPUT) {
            // For clearPendingEchoEvents (timeout == 0), return false
            if (timeout.count() == 0) {
                return pendingEvents_.size() > 0;
            }

            // Simulate echo response: wait a bit then generate events
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            // Generate rising + falling edge pair simulating ~12cm distance
            // 12cm = pulseUs * 0.0343 / 2 → pulseUs ≈ 700µs
            auto baseTime = now_ns();
            pendingEvents_.clear();
            pendingEvents_.push_back(
                edge_event(edge_event::event_type::RISING_EDGE, baseTime));
            pendingEvents_.push_back(
                edge_event(edge_event::event_type::FALLING_EDGE,
                           baseTime + 700000));  // +700µs = ~12cm

            return true;
        }

        if (mode_ == request_mode::FLOW_INPUT) {
            // Simulate ~10 Hz flow pulses
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return true;
        }

        return false;
    }

    // read_edge_events — returns simulated events
    std::size_t read_edge_events(edge_event_buffer& buf) {
        buf.clear();

        if (mode_ == request_mode::ECHO_INPUT) {
            // Return one event at a time from pending
            if (!pendingEvents_.empty()) {
                buf.push_event(pendingEvents_.front());
                pendingEvents_.erase(pendingEvents_.begin());
                return 1;
            }
            return 0;
        }

        if (mode_ == request_mode::FLOW_INPUT) {
            // Single falling edge for flow meter
            buf.push_event(edge_event(
                edge_event::event_type::FALLING_EDGE, now_ns()));
            return 1;
        }

        return 0;
    }

private:
    request_mode mode_;
    bool trigFired_ = false;
    uint64_t trigTime_ = 0;
    std::vector<edge_event> pendingEvents_;

    static uint64_t now_ns() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
    }
};

// ─── request_builder ─────────────────────────────────────────────────────────

class request_builder {
public:
    void set_consumer(const std::string& name) { consumer_ = name; }
    void set_line_config(const line_config& cfg) { cfg_ = cfg; }

    line_request do_request() {
        // Determine the request type based on consumer name and direction
        line_request::request_mode mode = line_request::request_mode::FLOW_INPUT;

        if (consumer_.find("trig") != std::string::npos) {
            mode = line_request::request_mode::TRIG_OUTPUT;
            std::cout << "  [MOCK] GPIO TRIG output configured\n";
        } else if (consumer_.find("echo") != std::string::npos) {
            mode = line_request::request_mode::ECHO_INPUT;
            std::cout << "  [MOCK] GPIO ECHO input configured (simulating ~12cm)\n";
        } else if (cfg_.settings_.dir_ == line::direction::OUTPUT) {
            mode = line_request::request_mode::TRIG_OUTPUT;
            std::cout << "  [MOCK] GPIO output line requested\n";
        } else {
            std::cout << "  [MOCK] GPIO input line requested (simulating pulses at ~10 Hz)\n";
        }

        return line_request(mode);
    }

private:
    std::string consumer_;
    line_config cfg_;
};

// ─── chip ────────────────────────────────────────────────────────────────────

class chip {
public:
    explicit chip(const std::string& path) {
        std::cout << "  [MOCK] Opened GPIO chip: " << path << "\n";
    }

    request_builder prepare_request() { return request_builder(); }
};

} // namespace gpiod

#endif // GPIOD_HPP_MOCK
