#pragma once
// Prevents the header from being included multiple times

#include <atomic>        // Thread-safe variables
#include <chrono>        // Timing operations
#include <functional>    // Callback function support
#include <memory>        // Smart pointers
#include <thread>        // Multithreading
#include <string>        // String handling
#include <stdexcept>     // Runtime error handling
#include <iostream>      // Console output

// If building on Raspberry Pi with real GPIO, define USE_HARDWARE_GPIO
// Otherwise, the sensor runs in simulation mode (0 flow, no hardware needed)
#ifdef USE_HARDWARE_GPIO
    #include <gpiod.hpp>     // GPIO access library for Raspberry Pi
#endif


/**
 * @brief YF-S401 Hall-effect Flow Sensor driver.
 *
 * Counts falling-edge pulses from the sensor's Hall-effect output,
 * then calculates flow rate using the calibration constant:
 *     flow_rate (L/min) = frequency (Hz) / calibration_factor
 *
 * Uses two threads:
 *   - edgeWorker: listens for GPIO falling edges (hardware mode only)
 *   - rateWorker: calculates flow rate every second
 *
 * Compile with -DUSE_HARDWARE_GPIO on Raspberry Pi for real GPIO.
 * Without it, the sensor runs in simulation mode (always 0 flow).
 */
class FlowSensor {
public:

    // Callback triggered on every pulse from the sensor
    using PulseCallback = std::function<void()>;

    // Callback triggered every second with calculated flow rate (L/min)
    using FlowCallback  = std::function<void(float)>;


    // Constructor
    //   chipNo:  GPIO chip number (e.g. 4 for /dev/gpiochip4 on Pi 5)
    //   pinNo:   GPIO line number (e.g. 23 for GPIO23)
    //   pulsesPerLitrePerMin: calibration constant (YF-S401 ≈ 98)
    FlowSensor(unsigned int chipNo,
               unsigned int pinNo,
               float pulsesPerLitrePerMin = 98.0f)
        : chipNo_(chipNo),
          pinNo_(pinNo),
          calibrationFactor_(pulsesPerLitrePerMin) {}


    // Destructor — stops threads automatically when object is destroyed
    ~FlowSensor() {
        stop();
    }


    // Register a callback that fires on every pulse (optional, good for debugging)
    void registerPulseCallback(PulseCallback cb) {
        pulseCallback_ = std::move(cb);
    }


    // Register a callback that fires every second with the flow rate in L/min
    void registerFlowCallback(FlowCallback cb) {
        flowCallback_ = std::move(cb);
    }


    // ── Start / Stop ─────────────────────────────────────────────────────────

    // Start the sensor — sets up GPIO and launches worker threads
    void start() {

        if (running_) return;  // Prevent starting twice

#ifdef USE_HARDWARE_GPIO
        setupGpioRequest();    // Configure GPIO pin for falling-edge detection
#else
        std::cout << "[SIM] No hardware GPIO — running in simulation mode (0 flow)\n";
#endif

        running_ = true;       // Mark system as running
        pulseCount_ = 0;       // Reset pulse counter
        sampleStart_ = std::chrono::steady_clock::now(); // Start timing

#ifdef USE_HARDWARE_GPIO
        // Launch edge detection thread (listens for sensor pulses)
        edgeThread_ = std::thread(&FlowSensor::edgeWorker, this);
#endif

        // Launch flow rate calculation thread (computes L/min every second)
        rateThread_ = std::thread(&FlowSensor::rateWorker, this);
    }


    // Stop the sensor — signals threads to exit and waits for them
    void stop() {

        if (!running_) return;

        running_ = false;  // Signal threads to exit

#ifdef USE_HARDWARE_GPIO
        // Wait for edge detection thread to finish
        if (edgeThread_.joinable())
            edgeThread_.join();
#endif

        // Wait for flow calculation thread to finish
        if (rateThread_.joinable())
            rateThread_.join();
    }


    // Returns the current cumulative pulse count (thread-safe)
    unsigned long getPulseCount() const {
        return pulseCount_.load();
    }


private:

    // ── GPIO Setup (hardware mode only) ──────────────────────────────────────

#ifdef USE_HARDWARE_GPIO

    // Configure GPIO pin as input with falling-edge detection
    void setupGpioRequest() {

        const std::string chipPath = "/dev/gpiochip" + std::to_string(chipNo_);
        const std::string consumer = "flow_sensor_gpio";

        // Open the GPIO chip
        chip_ = std::make_shared<gpiod::chip>(chipPath);

        // Configure pin: input direction, detect falling edges
        gpiod::line_config lineCfg;
        lineCfg.add_line_settings(
            {pinNo_},
            gpiod::line_settings{}
                .set_direction(gpiod::line::direction::INPUT)
                .set_edge_detection(gpiod::line::edge::FALLING)
        );

        // Request access to the GPIO line
        auto builder = chip_->prepare_request();
        builder.set_consumer(consumer);
        builder.set_line_config(lineCfg);
        request_ = std::make_shared<gpiod::line_request>(builder.do_request());
    }

    // ── Edge Detection Thread (hardware mode only) ───────────────────────────

    // Listens for falling-edge GPIO events and increments pulse counter
    void edgeWorker() {

        while (running_) {

            // Block until edge event or 200ms timeout
            bool eventReady =
                request_->wait_edge_events(std::chrono::milliseconds(200));

            if (!running_) break;
            if (!eventReady) continue;  // Timeout — no event

            // Read all available edge events into buffer
            gpiod::edge_event_buffer buffer(8);
            std::size_t numEvents = request_->read_edge_events(buffer);

            // Process each event
            for (std::size_t i = 0; i < numEvents; ++i) {

                const auto& ev = buffer.get_event(i);

                // Only count falling edges (one per rotation of the sensor)
                if (ev.event_type() ==
                    gpiod::edge_event::event_type::FALLING_EDGE) {

                    pulseCount_++;  // Atomic increment

                    // Notify pulse callback if registered
                    if (pulseCallback_) {
                        pulseCallback_();
                    }
                }
            }
        }
    }
#endif

    // ── Flow Rate Calculation Thread ─────────────────────────────────────────

    // Calculates flow rate every second from accumulated pulses
    //   flow_rate = (pulses / elapsed_seconds) / calibration_factor
    void rateWorker() {

        while (running_) {

            // Wait 1 second between calculations
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Measure elapsed time since last sample
            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<float> dt = now - sampleStart_;
            sampleStart_ = now;

            // Get pulse count and reset to 0 atomically
            unsigned long pulses = pulseCount_.exchange(0);

            // Calculate pulse frequency (Hz)
            float frequency =
                (dt.count() > 0.0f)
                ? (static_cast<float>(pulses) / dt.count())
                : 0.0f;

            // Convert frequency to flow rate (L/min)
            float flowRate = frequency / calibrationFactor_;

            // Send result through callback if registered
            if (flowCallback_) {
                flowCallback_(flowRate);
            }
        }
    }

    // ── Member Variables ─────────────────────────────────────────────────────

private:

    unsigned int chipNo_;         // GPIO chip number (e.g. 0 or 4)
    unsigned int pinNo_;          // GPIO pin number (e.g. 23)
    float calibrationFactor_;     // Pulses per litre per minute (YF-S401 ≈ 98)

    std::atomic<bool> running_{false};           // Thread-safe running flag
    std::atomic<unsigned long> pulseCount_{0};   // Thread-safe pulse counter

    std::chrono::steady_clock::time_point sampleStart_;  // Last measurement time

    PulseCallback pulseCallback_;   // Called on every pulse (optional)
    FlowCallback flowCallback_;     // Called every second with flow rate

    std::thread rateThread_;        // Flow calculation thread

#ifdef USE_HARDWARE_GPIO
    std::thread edgeThread_;        // Edge detection thread

    std::shared_ptr<gpiod::chip> chip_;            // GPIO chip handle
    std::shared_ptr<gpiod::line_request> request_;  // GPIO line handle
#endif
};
