#pragma once

#include "IHardwareDevice.h"
#include "IProximitySensor.h"
#include <thread>
#include <atomic>
#include <string>
#include <cstdint>
#include <cstdint>  // uint8_t
// timerfd_create / timerfd_settime (replaces sleep-based polling)
#include <sys/timerfd.h>

class GestureSensor : public IHardwareDevice, public IProximitySensor {
public:
    GestureSensor(int i2cBus = 1, int i2cAddr = 0x39, int threshold = 40);
    ~GestureSensor() override;

    GestureSensor(const GestureSensor&) = delete;
    GestureSensor& operator=(const GestureSensor&) = delete;

    bool init() override;
    void shutdown() override;

    void registerEventCallback(IProximitySensor::EventCallback cb) override;
    void registerErrorCallback(IProximitySensor::ErrorCallback cb) override;

private:
    void worker();
    uint8_t readRegister(uint8_t reg);
    void writeRegister(uint8_t reg, uint8_t value);

    int i2cBus_;
    int i2cAddr_;
    int threshold_;
    int fd_{-1};       ///< I2C file descriptor
    int timerFd_{-1};  ///< timerfd used for RTES-compliant blocking wait (replaces sleep)

    std::atomic<bool> running_{false};
    std::thread workerThread_;

    IProximitySensor::EventCallback eventCallback_;
    IProximitySensor::ErrorCallback errorCallback_;

    // Gesture decoding buffer
    uint8_t first_u_{0}, first_d_{0}, first_l_{0}, first_r_{0};
    uint8_t last_u_{0}, last_d_{0}, last_l_{0}, last_r_{0};
    int gesture_dataset_count_{0};
    bool gesture_active_{false};
};
