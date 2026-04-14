#include "hardware/GestureSensor.h"
#include "utils/Logger.h"
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cmath>
#include <stdexcept>

#define APDS9960_ID 0x92
#define APDS9960_ENABLE 0x80
#define APDS9960_PDATA 0x9C
#define APDS9960_GCONF4 0xAB
#define APDS9960_GCONF2 0xA3
#define APDS9960_GPENTH 0xA0
#define APDS9960_GEXTH  0xA1
#define APDS9960_GSTATUS 0xAF
#define APDS9960_GFLVL  0xAE
#define APDS9960_GFIFO_U 0xFC
#define APDS9960_GFIFO_D 0xFD
#define APDS9960_GFIFO_L 0xFE
#define APDS9960_GFIFO_R 0xFF

#define BRAND_ID_ADAFRUIT_1 0xAB
#define BRAND_ID_ADAFRUIT_2 0xA8
#define BRAND_ID_DOLLATEK 0x9E

const int POLL_INTERVAL_MS = 50;

GestureSensor::GestureSensor(int i2cBus, int i2cAddr, int threshold)
    : i2cBus_(i2cBus), i2cAddr_(i2cAddr), threshold_(threshold) {}

GestureSensor::~GestureSensor() {
    shutdown();
}

bool GestureSensor::init() {
    if (running_) return true;

    try {
        const std::string devPath = "/dev/i2c-" + std::to_string(i2cBus_);
        fd_ = open(devPath.c_str(), O_RDWR);
        if (fd_ < 0) {
            throw std::runtime_error("Cannot open " + devPath + ": " + strerror(errno));
        }

        if (ioctl(fd_, I2C_SLAVE, i2cAddr_) < 0) {
            throw std::runtime_error("ioctl I2C_SLAVE failed: " + std::string(strerror(errno)));
        }

        uint8_t id = readRegister(APDS9960_ID);
        if (id != BRAND_ID_ADAFRUIT_1 && id != BRAND_ID_ADAFRUIT_2 && id != BRAND_ID_DOLLATEK) {
            Logger::warn("Unknown APDS9960 device ID: " + std::to_string(static_cast<int>(id)));
        }

        // Configure Gesture Engine
        // Set enter and exit thresholds
        writeRegister(APDS9960_GPENTH, 50); // Enter gesture state when prox > 50
        writeRegister(APDS9960_GEXTH, 40);  // Exit gesture state when prox < 40
        
        // GCONF2: GGAIN=4x, GLDRIVE=100mA, GWTIME=2.8ms
        writeRegister(APDS9960_GCONF2, 0x41);

        // Turn on Power(0x01), Proximity(0x04), and Gesture(0x40) engines
        // GEN + PEN + PON = 0x45
        writeRegister(APDS9960_ENABLE, 0x45);

        running_ = true;

        // ── RTES: timerfd-driven I2C sampling ──────────────────────────────────
        // The APDS-9960 INT pin is not wired; instead a timerfd fires every
        // POLL_INTERVAL_MS. The worker thread blocks on ::read(timerFd_) — a
        // true kernel sleep consuming zero CPU — then checks GSTATUS on wake
        // to confirm data validity before reading proximity / gesture FIFOs.
        timerFd_ = timerfd_create(CLOCK_MONOTONIC, 0);
        if (timerFd_ < 0)
            throw std::runtime_error("GestureSensor: timerfd_create failed");

        struct itimerspec its{};
        its.it_value.tv_sec     = 0;
        its.it_value.tv_nsec    = POLL_INTERVAL_MS * 1'000'000L;  // first fire
        its.it_interval.tv_sec  = 0;
        its.it_interval.tv_nsec = POLL_INTERVAL_MS * 1'000'000L;  // repeat
        if (timerfd_settime(timerFd_, 0, &its, nullptr) < 0)
            throw std::runtime_error("GestureSensor: timerfd_settime failed");

        workerThread_ = std::thread(&GestureSensor::worker, this);
        return true;
    } catch (const std::exception& e) {
        if (errorCallback_) errorCallback_(e.what());
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
        return false;
    }
}

void GestureSensor::shutdown() {
    if (!running_) return;
    running_ = false;

    // Close timerfd first so the blocking read() in the worker unblocks
    if (timerFd_ >= 0) {
        close(timerFd_);
        timerFd_ = -1;
    }

    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    if (fd_ >= 0) {
        // Power off
        writeRegister(APDS9960_ENABLE, 0x00);
        close(fd_);
        fd_ = -1;
    }
}

void GestureSensor::registerEventCallback(EventCallback cb) {
    eventCallback_ = std::move(cb);
}

void GestureSensor::registerErrorCallback(ErrorCallback cb) {
    errorCallback_ = std::move(cb);
}

#ifdef AQUAFLOW_TESTING
void GestureSensor::emitEventForTest(const GestureEvent& event) const {
    if (eventCallback_) {
        eventCallback_(event);
    }
}
#endif

void GestureSensor::worker() {
    bool isTriggered = false;

    while (running_) {
        try {
            // 1. Evaluate Proximity
            int prox = readRegister(APDS9960_PDATA);
            bool currentlyTriggered = (prox > threshold_);

            if (currentlyTriggered && !isTriggered) {
                isTriggered = true;
                if (eventCallback_) {
                    eventCallback_({ProximityState::PROXIMITY_TRIGGERED, GestureDir::NONE, prox});
                }
            } else if (!currentlyTriggered && isTriggered) {
                int hysteresis_lower = (threshold_ - 20 >= 0) ? (threshold_ - 20) : 0;
                if (prox < hysteresis_lower) {
                    isTriggered = false;
                    if (eventCallback_) {
                        eventCallback_({ProximityState::PROXIMITY_CLEARED, GestureDir::NONE, prox});
                    }
                }
            }

            // 2. Evaluate Gesture FIFOs
            uint8_t gstatus = readRegister(APDS9960_GSTATUS);
            if (gstatus & 0x01) { // GVALID - Data is ready
                uint8_t gflvl = readRegister(APDS9960_GFLVL); // Amount of dataset in FIFO
                for (int i = 0; i < (int)gflvl; i++) {
                    uint8_t u = readRegister(APDS9960_GFIFO_U);
                    uint8_t d = readRegister(APDS9960_GFIFO_D);
                    uint8_t l = readRegister(APDS9960_GFIFO_L);
                    uint8_t r = readRegister(APDS9960_GFIFO_R);

                    // Only process frames that show strong reflections (filter noise floor)
                    if (std::abs((int)u - (int)d) > 13 || std::abs((int)l - (int)r) > 13) {
                       if (gesture_dataset_count_ == 0) {
                           // Record the exact entry moment
                           first_u_ = u; first_d_ = d; first_l_ = l; first_r_ = r;
                       }
                       // Continually overwrite to get the exact exit moment
                       last_u_ = u; last_d_ = d; last_l_ = l; last_r_ = r;
                       gesture_dataset_count_++;
                    }
                }
                gesture_active_ = true;
            } else if (gesture_active_) {
                // Buffer is empty, meaning the gesture has finished. Evaluate direction!
                
                // Must have seen at least 4 valid frames to count as a human gesture (eliminates phantom UP/DOWN)
                if (gesture_dataset_count_ > 4) { 
                    int ud_first = first_u_ - first_d_;
                    int lr_first = first_l_ - first_r_;
                    int ud_last = last_u_ - last_d_;
                    int lr_last = last_l_ - last_r_;

                    // Vector Derivative: Final Position - Initial Position
                    // This mathematically neutralizes ALL ambient hardware case reflections!
                    int ud_delta = ud_last - ud_first;
                    int lr_delta = lr_last - lr_first;

                    GestureDir dir = GestureDir::NONE;

                    // Which axis changed the most structurally from start to end?
                    if (std::abs(ud_delta) > std::abs(lr_delta)) {
                        if (ud_delta > 0) dir = GestureDir::UP; // Went from D to U
                        else dir = GestureDir::DOWN; // Went from U to D
                    } else {
                        if (lr_delta > 0) dir = GestureDir::LEFT; // Went from R to L
                        else dir = GestureDir::RIGHT; // Went from L to R
                    }

                    if (dir != GestureDir::NONE && eventCallback_) {
                        eventCallback_({ProximityState::NONE, dir, prox});
                    }
                }

                // Reset for next swipe
                gesture_dataset_count_ = 0;
                gesture_active_ = false;
            }

        } catch (const std::exception& e) {
            if (errorCallback_) errorCallback_(e.what());
        }

        // ── RTES-compliant blocking wait (timerfd) ───────────────────────────
        // Thread blocks here until the kernel fires the timerfd at the next
        // POLL_INTERVAL_MS boundary — zero CPU while idle, no sleep() call.
        // On wake, I2C registers are read and GSTATUS checked for data validity.
        // If timerFd_ was closed by shutdown(), read() returns -1 → loop exits.
        uint64_t expirations = 0;
        if (::read(timerFd_, &expirations, sizeof(expirations)) < 0) {
            break;  // timerfd closed by shutdown() — clean exit
        }
    }
}

uint8_t GestureSensor::readRegister(uint8_t reg) {
    if (::write(fd_, &reg, 1) != 1) {
        throw std::runtime_error("Failed to write register address");
    }
    uint8_t val;
    if (::read(fd_, &val, 1) != 1) {
        throw std::runtime_error("Failed to read register value");
    }
    return val;
}

void GestureSensor::writeRegister(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    if (::write(fd_, buf, 2) != 2) {
        throw std::runtime_error("Failed to write to register");
    }
}
