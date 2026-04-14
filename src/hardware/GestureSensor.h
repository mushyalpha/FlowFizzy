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

/**
 * @brief Struct to hold gesture and proximity event data delivered to observers.
 */
struct GestureEvent {
    ProximityState state;          ///< State indicating if cup is triggered or cleared
    GestureDir direction;          ///< Enumerated gesture direction detected
    int proximityValue;            ///< Raw 8-bit analog proximity reading
};

class GestureSensor : public IHardwareDevice {
public:
    /**
     * @brief Signature for successful gesture/proximity events.
     * @param event The populated GestureEvent instance to process.
     */
    using EventCallback = std::function<void(const GestureEvent&)>;

    /**
     * @brief Signature for error and system breakdown callbacks.
     * @param errorMsg The explicit string defining the failure.
     */
    using ErrorCallback = std::function<void(const std::string&)>;

    /**
     * @brief Initializes the APDS-9960 sensor via I2C interface.
     * @param i2cBus The I2C bus channel integer (e.g. 1 for /dev/i2c-1).
     * @param i2cAddr The hex pointer mapping to the sensor logic.
     * @param threshold Minimum analog reading to trigger proximity (0-255).
     */
    GestureSensor(int i2cBus = 1, int i2cAddr = 0x39, int threshold = 40);
    ~GestureSensor() override;

    /** @brief Allocates timerfd and launches worker I2C-polling thread. */
    bool init() override;

    /** @brief Reaps polling thread and safely closes mapped file descriptors. */
    void shutdown() override;

    /**
     * @brief Sets primary handler for reacting to external proximity events.
     * @param cb Stored event callback, executes immediately inline.
     */
    void registerEventCallback(EventCallback cb);

    /**
     * @brief Sets fallback handler reacting to kernel I2C exceptions.
     * @param cb Stored error execution function string log.
     */
    void registerErrorCallback(ErrorCallback cb);

#ifdef AQUAFLOW_TESTING
    /** @brief Test seam for unit tests to inject a synthetic event. */
    void emitEventForTest(const GestureEvent& event) const;
#endif

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

    EventCallback eventCallback_;
    ErrorCallback errorCallback_;

    // Gesture decoding buffer
    uint8_t first_u_{0}, first_d_{0}, first_l_{0}, first_r_{0};
    uint8_t last_u_{0}, last_d_{0}, last_l_{0}, last_r_{0};
    int gesture_dataset_count_{0};
    bool gesture_active_{false};
};
