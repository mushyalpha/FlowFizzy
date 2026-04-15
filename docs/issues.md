# Project Issue Tracker (Offline)

This document tracks technical issues and milestones for the AquaFlow project. 
*Note: These are synced to GitHub Issues periodically.*

## Demo Configuration

> **Liquid:** Pepsi
> **Cups:** McDonald's standard soft-drink cups
> - Small  → 250 ml
> - Medium → 400 ml
> - Large  → 500 ml
>
> These volumes are already configured in `PinConfig.h` as `CUP_SMALL_ML`, `CUP_MEDIUM_ML`, and `CUP_LARGE_ML`.

---

## Open Issues

### [#19] Volume Calibration: McDonald's Cup Sizes + LCD Validation
- **Status:** Open
- **Priority:** Critical
- **Labels:** `calibration`, `hardware`, `lcd`
- **Description:** The current volume targets (`CUP_SMALL_ML=250`, `CUP_MEDIUM_ML=400`, `CUP_LARGE_ML=500`) are based on nominal McDonald's UK cup specs, not measured Pepsi fill levels. Real-world accuracy depends on the flow meter's `ML_PER_PULSE` calibration and actual cup capacity under normal fill conditions.
- **Tasks to complete:**
  - [ ] Measure the actual usable volume of each McDonald's cup size (Small / Medium / Large) with Pepsi.
  - [ ] Run a dispense test for each size, weigh or measure the dispensed liquid, and adjust `CUP_SMALL_ML`, `CUP_MEDIUM_ML`, `CUP_LARGE_ML` in `include/PinConfig.h` accordingly.
  - [ ] Re-verify `ML_PER_PULSE` calibration constant — current value is `0.073774` ml/pulse (measured 2026-04-02 with water; Pepsi may behave slightly differently due to density/viscosity).
  - [ ] Confirm the LCD second row shows the correct target ml for each size and that the live fill progress accurately reflects the dispensed volume.

---

### [#20] 3D Printing: Enclosure and Mounting Design
- **Status:** Open
- **Priority:** High
- **Labels:** `hardware`, `mechanical`, `design`
- **Description:** The system currently has no physical enclosure or mounting structure. All hardware components — the Raspberry Pi 5, pump reservoir, flow meter, LCD display, and APDS-9960 sensor — need to be mounted in a unified, stable, and visually presentable housing suitable for a demo.
- **Design requirements:**
  - [ ] Reservoir mount to hold the Pepsi bottle/container above the pump.
  - [ ] Sensor bay aligned at cup-top height for reliable proximity detection (~5–10 cm range).
  - [ ] Secure bracket for the LCD1602 display, facing the user.
  - [ ] Cable management channels for I2C, GPIO, and power wires.
  - [ ] Dispense nozzle guide to direct water flow accurately into McDonald's cups.
  - [ ] Raspberry Pi 5 mounting with ventilation clearance.
- **Tools:** CAD software (e.g., Fusion 360 / FreeCAD / Tinkercad); 3D printer (PLA recommended).

---



### [#16] APDS9960: Gestures not detected & Proximity range degraded in C++ port
- **Status:** Open
- **Priority:** Critical
- **Labels:** `bug`, `hardware`
- **Description:** After porting from the Adafruit CircuitPython APDS9960 library to our custom C++ driver, gesture detection has stopped working and proximity range has dropped from ~20 cm to ~2–5 cm. Seven root causes identified via comparison with the reference Python library — see [`docs/github_issue_apds9960_draft.md`](github_issue_apds9960_draft.md) for full analysis and proposed fix.

---

### [#15] Finalise Unit Testing for FillingController
- **Status:** Open
- **Priority:** High
- **Description:** Need to verify state transitions in `FillingController` using a Mock FlowMeter.

### [#18] Safety Enhancement: Distinguish Cup Presence from Empty Hand Waves
- **Status:** Open
- **Priority:** High
- **Labels:** `enhancement`, `safety`
- **Description:** Currently, the APDS-9960 sensor serves as both the proximity trigger (cup detector) and the gesture detector. If a user waves their hand rapidly across the empty sensor, the hand itself triggers proximity (`bottlePresent_` = true) and registers a gesture, causing the pump to briefly turn on before the hand exits and triggers the safety abort. We need to implement a stricter safety condition (e.g., integrating a dedicated IR/Ultrasonic cup sensor, or requiring the proximity signal to be completely static for *N* seconds before accepting gestures) to mathematically guarantee that water never flows unless a physical cup is definitively present.

---

## Closed Issues

### [#17] Unit Test Compilation Failure (libgpiod mock API divergence)
- **Status:** Closed
- **Description:** `make -j4` failed when compiling `filling_tests` because `src/hardware/FlowMeter.cpp` was built against a mock version of `gpiod.hpp` (`tests/mock_include/gpiod.hpp`) that did not fully match the real `libgpiod` v2 C++ bindings on the Raspberry Pi 5. Specifically, the mock lacked the `set_bias` method, missing `bias` enumerations, and the correct `add_line_settings()` overload for single integers. It also caused an ambiguous resolution for the edge event `type()` method.
- **Resolved:** Fixed `mock_include/gpiod.hpp` to perfectly emulate the real `libgpiod` v2 API used by the system hardware. This allowed both the hardware integration targets and the unit test framework (`filling_tests`) to compile identically against the same `FlowMeter.cpp` logic.

### [#14] Replace std::cout with Logger in state machine
- **Status:** Closed (Linked in commit 9e1f565)
- **Resolved:** Removed non-deterministic I/O from `FillingController::tick()`.

### [#13] Implement acquire/release memory ordering for proximity flag
- **Status:** Closed (Linked in commit 9e1f565)
- **Resolved:** Improved thread safety for the `bottlePresent_` atomic.

### [#12] Fix GestureSensor POLL_INTERVAL
- **Status:** Closed (Linked in commit 10d415f)
- **Resolved:** Reverted to 50ms for better responsiveness.

### [#11] Add timerfd-based blocking to GestureSensor
- **Status:** Closed
- **Resolved:** Replaced `sleep_for` with kernel-managed timerfd.

### [#10] Migrate Pump to libgpiod v2.x
- **Status:** Closed
- **Resolved:** Updated hardware drivers for Raspberry Pi 5 compatibility.

### [#1] Initial Project Structure
- **Status:** Closed
- **Resolved:** Created hardware, state, and monitor directories.
