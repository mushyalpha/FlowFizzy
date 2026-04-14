# AquaFlow Water Dispenser

AquaFlow is a smart, fully-automated touchless water dispenser built on the Raspberry Pi using C++. It utilizes intelligent hardware monitoring to safely dispense exact volumes of water using proximity detection.

For SOLID compliance, `FillingController` now depends on behavioral interfaces (`IProximitySensor`, `IPump`, `IFlowMeter`) instead of concrete drivers. We considered a template-based variant for zero-overhead static polymorphism, but chose runtime interfaces for clearer architecture and easier assessment traceability; at a 100 ms control interval, virtual dispatch overhead is negligible.

## Hardware Connections (Raspberry Pi Pinout)

Below is the definitive hardware wiring guide to connect the sensors and pump to the Raspberry Pi.
> **Note:** Always ensure the Raspberry Pi is powered OFF when altering hardware connections. 

### 1. Gesture/Proximity Sensor (DollaTek APDS-9960)
This acts as the touchless cup detector. It communicates via the I2C protocol natively over `3.3V`.
| Sensor Pin | Raspberry Pi Pin | Function |
| :--- | :--- | :--- |
| **VCC** | **Pin 1 (3.3V)** | Main Power |
| **GND** | **Pin 6 (GND)** | Ground |
| **SDA** | **Pin 3 (GPIO 2)** | I2C Data Line |
| **SCL** | **Pin 5 (GPIO 3)** | I2C Clock Line |
| **VL** | **Pin 17 (3.3V)** (or solder to VCC) | Powers the IR LED for proximity |

### 2. Water Flow Sensor (YF-S401)
This sensor spins as water traverses it, sending digital pulses to precisely measure volume.
| Sensor Wire Color | Raspberry Pi Pin | Function |
| :--- | :--- | :--- |
| **Red** | **Pin 2 (5V)** | Power |
| **Black** | **Pin 9 (GND)** | Ground |
| **Yellow** | **Pin 11 (GPIO 17)** | Pulse Signal Line (Open Collector) |

### 3. DC Submersible Pump (JT80SL via TIP122 Transistor)
Because the pump draws more current than the Pi can provide, it must be driven via a transistor circuit (using the Darlington TIP122) and protected by a flyback diode. 

Here is the exact breadboard wiring layout:

*   **TIP122 Base (Pin 1 - connected at 11a):**
    *   One end of a **1k resistor** connects to `11c`.
    *   The other end of the resistor connects to `19f`.
    *   A wire from `19g` connects directly to **GPIO 18** on the Raspberry Pi.
*   **TIP122 Collector (Pin 2 - connected at 12a):**
    *   The pump's **black wire** connects to `12e`.
    *   The flyback diode's **non-striped side** connects to `12c`.
*   **Power Rail (Positive voltage - Row 6):**
    *   The pump's **red wire** connects to `6e`.
    *   The **5V supply wire** (from Pi or **External Powerbank Positive**) connects to `6d`.
    *   The flyback diode's **silver-striped side** connects to `6c`.
*   **TIP122 Emitter (Pin 3 - connected at 13a):**
    *   A wire runs from `13e` directly to a **Ground (GND)** pin on the Raspberry Pi.
    *   **⚡ CRITICAL:** If using an external powerbank, you MUST also connect the **Powerbank Ground (Negative) to the Pi's Ground**. Without this "common ground", the transistor circuit will not switch and the Pi could be damaged!

*(Note: The diode placed backwards between the Collector and the Power rail acts as a flyback diode. This prevents high-voltage "kickback" spikes from destroying the Raspberry Pi when the pump motor abruptly turns off).*

### 4. LCD Display (Optional I2C Backpack)
If utilizing the I2C LCD to display volume and status, you can safely share the I2C pins with the Gesture Sensor because they have different I2C addresses.
| LCD Pin | Raspberry Pi Pin | Function |
| :--- | :--- | :--- |
| **VDD** | **Pin 2 or 4 (5V)** | Power |
| **GND** | **GND** | Ground |
| **SDA** | **Pin 3 / GPIO 2** | Shared I2C Data Line |
| **SCL** | **Pin 5 / GPIO 3** | Shared I2C Clock Line |

## Build Instructions (Linux / RPi)

Make sure you have CMake and the C++ compilation tools installed. The project relies on `libgpiodcxx` for secure, race-free GPIO management.

```bash
mkdir build
cd build
cmake ..
make hardware_trio_test
sudo ./hardware_trio_test
```
