# System Architecture

This document presents the overarching software design, sequence flows, and exact components mapping that define the AquaFlow touchless automated dispenser system.

---

## 1. Class Diagram

The following diagram demonstrates the core **SOLID** structural choices made for this system.
Note the heavy reliance on **Dependency Inversion (D)**: The `FillingController` acts only on abstractions (`IHardwareDevice` inputs mapped as concrete injected instances at run-time). It is completely unaware of Linux/GPIO specifics.

```mermaid
classDiagram
    class IHardwareDevice {
        <<interface>>
        +init() bool
        +shutdown() void
    }

    class FlowMeter {
        -atomic~int~ pulseCount_
        -thread edgeThread_
        +getPulseCount() int
        +getVolumeML() double
        +hasReachedTarget(double) bool
    }

    class PumpController {
        -bool running_
        -int pumpPin_
        +turnOn() void
        +turnOff() void
        +isRunning() bool
    }

    class GestureSensor {
        -int i2cAddr_
        -timerFd_
        +registerEventCallback(cb) void
    }

    class FillingController {
        -SystemState state_
        -double targetVolumeML_
        +tick() void
        +registerMonitor(cb) void
    }

    class Monitor {
        +onStateChange(string, double, int)
    }

    class LcdDisplay {
        +printStatus(string, double, int)
    }

    IHardwareDevice <|-- FlowMeter
    IHardwareDevice <|-- PumpController
    IHardwareDevice <|-- GestureSensor
    IHardwareDevice <|-- LcdDisplay
    
    FillingController --> GestureSensor : uses
    FillingController --> PumpController : uses
    FillingController --> FlowMeter : uses
    
    Monitor ..> FillingController : observer
    LcdDisplay ..> Monitor : bound callback
```

---

## 2. Sequence Diagram

This sequence traces the primary temporal flow of operations: From the user placing their cup, through proximity callbacks and flow counting, closing off back to an idle state.

```mermaid
sequenceDiagram
    actor User
    participant GS as Gesture Sensor (Thread)
    participant FC as FillingController
    participant T as Timer (Main Tick)
    participant P as PumpController
    participant FM as FlowMeter (Thread)
    participant L as LcdDisplay
    
    User->>GS: Place cup near sensor
    activate GS
    GS-->>FC: Proximity Triggered Event (Store Atomic=True)
    deactivate GS
    
    loop Every 100ms
        T->>FC: tick()
        FC->>FC: STATE: WAITING -> CONFIRMATION
    end
    
    note over FC,T: Cup held for Hold Time (e.g. 2s)
    
    T->>FC: tick() (hold time complete)
    FC->>FM: resetCount()
    FC->>P: turnOn()
    FC->>FC: STATE: CONFIRMATION -> FILLING
    FC-->>L: Update Dislay (Filling)
    
    loop Real-Time Flow Monitoring
        FM->>FM: Hardware Pulses increment Volume (Async)
        T->>FC: tick()
        FC->>FM: hasReachedTarget(volume) -> False
        FC-->>L: Update Volume Dispensed
    end
    
    note over FM: Target Volume Reached
    T->>FC: tick()
    FC->>FM: hasReachedTarget(volume) -> True
    FC->>P: turnOff()
    FC->>FC: STATE: FILLING -> FILL_COMPLETE
    FC-->>L: Update Display (Done)
    
    User->>GS: Removes Cup
    GS-->>FC: Proximity Cleared Event
    
    T->>FC: tick()
    FC->>FM: resetCount()
    FC->>FC: STATE: FILL_COMPLETE -> WAITING
```

---

## 3. Physical Software Mapping Architecture

This diagram demonstrates how hardware interfaces directly communicate natively with our encapsulated classes to form the whole package.

```mermaid
graph TD
    subgraph Physical Hardware
        APDS9960[APDS-9960 Gesture/Prox]
        YFS[YF-S401 Flow Meter]
        PumpEngine[JT80SL Pump + TIP122]
        LCD[16x2 I2C LCD Display]
    end

    subgraph Hardware Abstraction Layer
        GS_Class[GestureSensor]
        FM_Class[FlowMeter]
        Pump_Class[PumpController]
        LCD_Class[LcdDisplay]
    end

    subgraph OS Protocols
        I2C1[I2C Bus 1]
        GPIO_Edge[libgpiod Edge Events]
        GPIO_Out[libgpiod Digital Output]
    end

    APDS9960 -->|I2C 3.3v| I2C1
    I2C1 --> GS_Class

    YFS -->|Pulses 5v| GPIO_Edge
    GPIO_Edge --> FM_Class

    Pump_Class --> GPIO_Out
    GPIO_Out -->|Digital High 3.3v| PumpEngine

    LCD_Class --> I2C1
    I2C1 -->|Commands| LCD
```
