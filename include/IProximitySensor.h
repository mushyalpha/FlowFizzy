#ifndef IPROXIMITYSENSOR_H
#define IPROXIMITYSENSOR_H

#include <functional>
#include <string>
#include <vector>

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
private:
    ProximityState state = ProximityState::NONE;
    GestureDir direction = GestureDir::NONE;

    // Backward-compatible primary reading.
    int proximityValue = 0;

    // Future-proof channel payload: derived classes can publish one or more
    // channels without changing the callback signature.
    std::vector<float> proximityChannels{};

public:
    GestureEvent() = default;
    
    GestureEvent(ProximityState s, GestureDir d, int pVal = 0, std::vector<float> channels = {})
        : state(s), direction(d), proximityValue(pVal), proximityChannels(std::move(channels)) {}

    ProximityState getState() const { return state; }
    GestureDir getDirection() const { return direction; }
    int getProximityValue() const { return proximityValue; }
    const std::vector<float>& getProximityChannels() const { return proximityChannels; }
};

class IProximitySensor {
public:
    // LSP note:
    // The callback signature remains stable while GestureEvent can evolve by
    // adding optional fields (such as proximityChannels), allowing richer
    // derived sensor implementations without changing base consumers.
    using EventCallback = std::function<void(const GestureEvent&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    virtual ~IProximitySensor() = default;
    virtual void registerEventCallback(EventCallback cb) = 0;
    virtual void registerErrorCallback(ErrorCallback cb) = 0;
};

#endif // IPROXIMITYSENSOR_H
