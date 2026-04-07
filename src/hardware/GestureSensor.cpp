#include "hardware/GestureSensor.h"
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

#define APDS9960_ID 0x92
#define APDS9960_ENABLE 0x80
#define APDS9960_PDATA 0x9C

#define BRAND_ID_ADAFRUIT_1 0xAB
#define BRAND_ID_ADAFRUIT_2 0xA8
#define BRAND_ID_DOLLATEK 0x9E

const int POLL_INTERVAL_MS = 100;

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

        // Enable power and proximity
        // Bit 0: PON (Power ON) = 1
        // Bit 2: PEN (Proximity ENable) = 1
        writeRegister(APDS9960_ENABLE, 0x05);

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
            int prox = readRegister(APDS9960_PDATA);

            bool currentlyTriggered = (prox > threshold_);

            if (currentlyTriggered && !isTriggered) {
                isTriggered = true;
                if (eventCallback_) {
                    eventCallback_({ProximityState::PROXIMITY_TRIGGERED, prox});
                }
            } else if (!currentlyTriggered && isTriggered) {
                int hysteresis_lower = (threshold_ - 20 >= 0) ? (threshold_ - 20) : 0;
                if (prox < hysteresis_lower) {
                    isTriggered = false;
                    if (eventCallback_) {
                        eventCallback_({ProximityState::PROXIMITY_CLEARED, prox});
                    }
                }
            }

        } catch (const std::exception& e) {
            if (errorCallback_) errorCallback_(e.what());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
    }
}

uint8_t GestureSensor::readRegister(uint8_t reg) {
    if (::write(fd_, &reg, 1) != 1) {
        throw std::runtime_error("Failed to write register address: " + std::string(strerror(errno)));
    }
    uint8_t val;
    if (::read(fd_, &val, 1) != 1) {
        throw std::runtime_error("Failed to read register value: " + std::string(strerror(errno)));
    }
    return val;
}

void GestureSensor::writeRegister(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    if (::write(fd_, buf, 2) != 2) {
        throw std::runtime_error("Failed to write to register: " + std::string(strerror(errno)));
    }
}
