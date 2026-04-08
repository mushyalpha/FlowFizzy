#pragma once

#include "IHardwareDevice.h"
#include <functional>
#include <thread>
#include <atomic>
#include <string>
#include <cstdint>

enum class ProximityState {
    NONE,
    PROXIMITY_TRIGGERED,
    PROXIMITY_CLEARED
};

enum class GestureDir {
    NONE,
    UP,
    DOWN,
    LEFT,
    RIGHT
};

struct GestureEvent {
    ProximityState state;
    GestureDir direction;
    int proximityValue;
};

class GestureSensor : public IHardwareDevice {
public:
    using EventCallback = std::function<void(const GestureEvent&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    GestureSensor(int i2cBus = 1, int i2cAddr = 0x39, int threshold = 40);
    ~GestureSensor() override;

    bool init() override;
    void shutdown() override;

    void registerEventCallback(EventCallback cb);
    void registerErrorCallback(ErrorCallback cb);

private:
    void worker();
    uint8_t readRegister(uint8_t reg);
    void writeRegister(uint8_t reg, uint8_t value);

    int i2cBus_;
    int i2cAddr_;
    int threshold_;
    int fd_{-1};
    
    std::atomic<bool> running_{false};
    std::thread workerThread_;

    EventCallback eventCallback_;
    ErrorCallback errorCallback_;

    // Gesture decoding buffer
    int gesture_ud_delta_{0};
    int gesture_lr_delta_{0};
    bool gesture_active_{false};
};
