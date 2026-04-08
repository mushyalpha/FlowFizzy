#include "hardware/GestureSensor.h"
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cmath>

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
            std::cerr << "Warning: Unknown APDS9960 device ID: 0x" << std::hex << (int)id << std::dec << "\n";
        }

        // Configure Gesture Engine
        // Set enter and exit thresholds
        writeRegister(APDS9960_GPENTH, 40); // Enter gesture state when prox > 40
        writeRegister(APDS9960_GEXTH, 30);  // Exit gesture state when prox < 30
        
        // GCONF2: GGAIN=4x, GLDRIVE=100mA, GWTIME=2.8ms
        writeRegister(APDS9960_GCONF2, 0x41);

        // Turn on Power(0x01), Proximity(0x04), and Gesture(0x40) engines
        // GEN + PEN + PON = 0x45
        writeRegister(APDS9960_ENABLE, 0x45);

        running_ = true;
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

                    // Calculate deltas
                    int ud = (int)u - (int)d;
                    int lr = (int)l - (int)r;
                    
                    // Filter noise
                    if (std::abs(ud) > 13) gesture_ud_delta_ += ud;
                    if (std::abs(lr) > 13) gesture_lr_delta_ += lr;
                }
                gesture_active_ = true;
            } else if (gesture_active_) {
                // Buffer is empty, meaning the gesture has finished. Evaluate direction!
                GestureDir dir = GestureDir::NONE;

                // Determine if it was primarily a vertical or horizontal swipe
                if (std::abs(gesture_ud_delta_) > std::abs(gesture_lr_delta_)) {
                    if (gesture_ud_delta_ > 30) dir = GestureDir::DOWN;
                    else if (gesture_ud_delta_ < -30) dir = GestureDir::UP;
                } else {
                    if (gesture_lr_delta_ > 30) dir = GestureDir::RIGHT;
                    else if (gesture_lr_delta_ < -30) dir = GestureDir::LEFT;
                }

                if (dir != GestureDir::NONE && eventCallback_) {
                    eventCallback_({ProximityState::NONE, dir, prox});
                }

                // Reset for next swipe
                gesture_ud_delta_ = 0;
                gesture_lr_delta_ = 0;
                gesture_active_ = false;
            }

        } catch (const std::exception& e) {
            if (errorCallback_) errorCallback_(e.what());
        }

        // Loop runs moderately fast to ensure we don't miss FIFO interrupts
        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
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
