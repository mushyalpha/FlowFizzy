# AquaFlow Architecture

## OCP Inheritance and Dependencies

This diagram documents the inheritance hierarchy and the main composition links relevant to Open/Closed Principle review.

```mermaid
classDiagram
    class IHardwareDevice {
      <<interface>>
      +bool init()
      +void shutdown()
    }
    class IProximitySensor {
      <<interface>>
      +registerEventCallback()
      +registerErrorCallback()
    }
    class IPump {
      <<interface>>
      +turnOn()
      +turnOff()
      +isRunning()
    }
    class IFlowMeter {
      <<interface>>
      +resetCount()
      +getPulseCount()
      +getVolumeML()
    }

    class PumpController
    class FlowMeter
    class GestureSensor
    class LcdDisplay
    class FillingController
    class Monitor
    class Timer
    class Logger

    IHardwareDevice <|-- PumpController
    IHardwareDevice <|-- FlowMeter
    IHardwareDevice <|-- GestureSensor
    IHardwareDevice <|-- LcdDisplay
    IProximitySensor <|-- GestureSensor
    IPump <|-- PumpController
    IFlowMeter <|-- FlowMeter

    FillingController --> IProximitySensor : registerEventCallback
    FillingController --> IPump : turnOn/turnOff
    FillingController --> IFlowMeter : reset/getVolume
    FillingController --> Monitor : registerMonitor(callback)
    main --> FillingController
    main --> Timer
    main --> LcdDisplay
    Monitor --> Logger
    PumpController --> Logger
    LcdDisplay --> Logger
```

## OCP Notes

- Hardware drivers are extendable through `IHardwareDevice` without changing the base interface.
- `FillingController` depends on behavioral abstractions (`IProximitySensor`, `IPump`, `IFlowMeter`) rather than concrete hardware drivers.
- New hardware implementations can be added by extending interfaces and wiring them in composition code without modifying controller logic.
