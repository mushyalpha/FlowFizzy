# Developer Journal

A chronological log of work done in the lab, daily goals, and simple tasks.

---

## 20 March 2026
**Time:** 14:16
**Location:** Rankine Lab 509

**Goals:**
- Identify the correct water pump and replace the solenoid valve setup.
- Figure out how to safely connect the pump to the Raspberry Pi.
- Resolve 5V power distribution across the various sensors and the pump.

**Notes:**
- We decided to switch back from the 12V Solenoid valve to a submersible pump because the solenoid was bulky and not performing as expected with the relay setup.
- Explored powering the pump. Tried direct pi connection (Bad Idea). Explored 3x 1.5v AA batteries (didn't go this route). Ultimately wired a T1P121 transistor with a flyback diode and a 1000 ohm resistor. 

---

## [Date TBD] 
**Time:** 11:23
**Location:** RTES lab

**Tasks:**
- [x] Connect the gesture sensor and have it working. (Read APDS-9960 specs at 11:26)
- [ ] Make a social media plan. We need to upload 5 times and get people to like.
- [ ] Research about the 3D printing.

**11:33 Subtask:**
- [ ] Find out how to connect the gesture sensor to the Raspberry Pi.

**Notes & Observations:**
- The wires we use are quite short and in some cases the connections are weak.
- Right now this is no big deal because we are testing our connections separately, but this will become important when we are bringing the product together.

---

## 14 April 2026

**Single Responsibility Review and Refactors**

### PumpController
- Responsibility: owns pump GPIO lifecycle and ON/OFF actuation.
- SRP decision: PASS.
- Changes made:
  - Replaced direct `std::cout` and `std::cerr` calls with `Logger::info()` and `Logger::error()`.
  - Replaced internal static relay flag with constructor-injected `DriveMode` (`RELAY_ACTIVE_LOW` or `TRANSISTOR_ACTIVE_HIGH`).
  - Default mode is `TRANSISTOR_ACTIVE_HIGH` to preserve current behaviour.

### FlowMeter
- Responsibility: detects flow pulses, debounces hardware noise, and converts pulses to millilitres.
- SRP decision: PASS.
- Changes made:
  - Removed `hasReachedTarget()` from `FlowMeter` to avoid business-rule leakage.
  - Kept `FlowMeter` as a measurement driver exposing `getPulseCount()` and `getVolumeML()` only.
  - Moved target-comparison logic into controllers/callers.

### LcdDisplay
- Responsibility: low-level 16x2 LCD rendering over I2C.
- SRP decision: PASS.
- Changes made:
  - Removed business-formatting methods (`showVolume()` and `showStatus()`).
  - Moved row text construction (`Vol:`, `Bottles:`) to `main.cpp` orchestrator logic.
  - Routed LCD init/shutdown logs through `Logger`.
  - Kept shutdown as hardware-only (backlight off and fd close); farewell text is now composed by caller before shutdown.

### GestureSensor
- Responsibility: read APDS9960 registers and emit proximity/gesture events.
- SRP decision: PASS.
- Changes made:
  - Replaced direct warning output with `Logger::warn()` for unknown device ID path.

### Monitor
- Responsibility: format and log state updates and transitions.
- SRP decision: PASS.
- Changes made:
  - Removed unused `registerCallback` API and internal callback member (YAGNI cleanup).
  - Assumed full responsibility for format and emission of human-readable state transition logs, making `FillingController` independent of UI/logging.

### FillingController
- Responsibility: Orchestrate hardware components for the filling cycle.
- SRP decision: PASS.
- Changes made:
  - Removed all `Logger` dependencies. All transition logs are deferred to `Monitor` for ultra-pure SRP.

### Final SRP Outcome
- All core classes now have one clear reason to change.
- Business rules and UI text formatting are kept in orchestration/presentation layers, not in hardware drivers.

### Open/Closed Principle Review (14 April 2026)
- `IHardwareDevice` remains the lifecycle extension point (`init()` and `shutdown()`), and `PumpController`, `FlowMeter`, `GestureSensor`, and `LcdDisplay` extend it without modifying the base class.
- Added role interfaces: `IProximitySensor`, `IPump`, and `IFlowMeter`.
- Refactored `FillingController` to depend only on those abstractions, not concrete hardware drivers.
- Evidence added: updated class diagram and hierarchy documentation in `docs/architecture.md`.
- Verdict: OCP/DIP pass at both hardware and orchestration layers.
