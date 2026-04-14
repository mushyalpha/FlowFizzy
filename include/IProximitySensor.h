#ifndef IPROXIMITYSENSOR_H
#define IPROXIMITYSENSOR_H

#include <functional>
#include <string>

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

class IProximitySensor {
public:
    using EventCallback = std::function<void(const GestureEvent&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    virtual ~IProximitySensor() = default;
    virtual void registerEventCallback(EventCallback cb) = 0;
    virtual void registerErrorCallback(ErrorCallback cb) = 0;
};

#endif // IPROXIMITYSENSOR_H
