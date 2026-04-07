# Architecture & Technical Decisions (ADRs)

A master log of technical bottlenecks and the resolutions implemented for the AquaFlow project.

---

## Decision 1: Pump vs Solenoid Valve
- **Problem:** The original setup utilized a 12V Solenoid valve. It looked solid and we bought a relay for it, but ultimately we couldn't get it working with our setup and it made the footprint too bulky.
- **Resolution:** Reverted to a DC submersible pump (JT80SL) instead of the solenoid valve.

## Decision 2: Managing Pump Power Draw
- **Problem:** The JT80SL draws up to 220mA, which is > 10x what a single Raspberry Pi GPIO pin can safely handle (16 mA max). 
- **Resolution:** We cannot connect it directly. Instead of using 3x 1.5v AA batteries, we wired a **T1P121 transistor with a flyback diode and a 1000 ohm resistor**. (Note: Consider switching to a MOSFET in the future).
- *Open Concern:* We need to establish if the concern about the 5V pin being over-occupied is a real concern and how to solve it.

## Decision 3: Resolving 5V Rail Contention
- **Problem:** We have 2 sensors, a pump, and LCDs, and they are all competing for the Pi's 5V rail. We used a relay module for the pump that took up two 5V pins.
- **Resolution Plan:** The pump should ideally be powered from a separate 5V supply rather than directly from the Pi. This avoids startup current spikes that can drop voltage and destabilize the Pi. Pi GPIO should only be used for control.
- **Proposed setup:** 
  - External 5V supply for pump.
  - Relay or MOSFET for switching.
  - Common ground shared with Raspberry Pi.
  *(Note: We already have a 12V power supply and can use an LM2596 Buck Converter).*

## Problems Still To Solve
- **Tubing Length & Cut:** We have 1m of 6mm tubing. Need to connect to both waterflow sensor and pump (both have inlet/outlet).
- **Positioning:** How much distance do we want between the pump and the water flow sensor? Does one of them need to submerge in water? Does distance to nozzle or water source matter? 
- **Sensor Voltage Dividers:** Ultrasonic sensor needs a voltage divider to connect safely to the Pi. Does the YF-S401 Flow Meter need one as well?
