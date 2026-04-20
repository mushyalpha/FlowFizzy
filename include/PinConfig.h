#ifndef PINCONFIG_H
#define PINCONFIG_H

// ─── GPIO Pin Assignments (BCM numbering) ────────────────────────────────────

// DC Water Pump (via TIP122 transistor)
constexpr int PUMP_PIN = 18;   // GPIO18 (BCM)

// YF-S401 Flow Meter
// Wiring: Red→Pin2(5V)  Black→Pin9(GND)  Yellow→Pin11(GPIO17)
// Signal is open-collector; internal pull-up enabled in FlowMeter::setupGpio()
constexpr int FLOW_PIN = 17;   // GPIO17 — BCM17 — Physical Pin 11

// Push-Button (short press = cycle size, long press = select)
// Physical button: GPIO26, Physical Pin 37.  Not yet installed.
// Keyboard simulation over SSH: 'b' = cycle size,  's' = select.
constexpr int BUTTON_PIN = 26;

// ─── Timing Settings ─────────────────────────────────────────────────────────
// State-machine tick rate.  Keep at 100 ms for stable proximity polling.
constexpr int LOOP_INTERVAL_MS = 100;   // ms

// Cup confirmation hold time — cup must remain in range this long before the
// pump starts.  Prevents false triggers while the user is still placing the cup.
// NOTE: 800ms was too fast during testing — the cup trigger fired mid-placement.
//       Increased to 1500ms (1.5s) to give the user time to set the cup down stably.
//       Adjust if the wait feels too long in production.
constexpr int CUP_CONFIRM_MS = 1500;  // ms

// ─── Flow Settings ───────────────────────────────────────────────────────────
// YF-S401 spec: ~5880 pulses/litre  →  1000 mL / 5880 pulses ≈ 0.1701 mL/pulse
//
// CALIBRATION HISTORY:
//   2026-04-02: 2711 pulses for 200 ml actual → 0.073774 ml/pulse
//   2026-04-20: System reported 111.7 ml when ~250 ml was actually dispensed.
//               Correction factor = 250 / 111.7 = 2.238
//               New value = 0.073774 × 2.238 = 0.1651 ml/pulse  ← INCORRECT
//   2026-04-20: Calibration tool (./calibrate) measured 4027 pulses for 260 ml actual.
//               New value = 260 / 4027 = 0.064561 ml/pulse
//               Previous 0.1651 was causing under-fill (~97 ml for a 250 ml target).
//
// To re-calibrate: run sudo ./calibrate in the build directory.
// Let the pump fill a known volume, press Ctrl+C, enter the actual ml.
constexpr double ML_PER_PULSE = 0.064564;  // mL per pulse — calibrated 2026-04-20 (tool: 4027 pulses = 260 ml)

// ─── Cup Size Volumes (McDonald's UK dispensing targets) ─────────────────────
constexpr double CUP_SMALL_ML  = 250.0;   // Small  (~8.5 fl oz)
constexpr double CUP_MEDIUM_ML = 400.0;   // Medium (~14.1 fl oz)
constexpr double CUP_LARGE_ML  = 500.0;   // Large  (~17.6 fl oz)

// Legacy alias — kept for test-file compatibility
constexpr double TARGET_VOLUME_ML = CUP_MEDIUM_ML;

// ─── GPIO Chip ───────────────────────────────────────────────────────────────
// /dev/gpiochip4 for Raspberry Pi 5; use 0 for Pi 1–4.
constexpr unsigned int GPIO_CHIP_NO = 4u;

// ─── I2C Bus / Addresses ─────────────────────────────────────────────────────
// APDS-9960: SDA→Pin3 (GPIO2), SCL→Pin5 (GPIO3), VCC→3.3V.  Address fixed 0x39.
constexpr int GESTURE_I2C_BUS  = 1;      // /dev/i2c-1
constexpr int GESTURE_I2C_ADDR = 0x39;   // APDS-9960 fixed I2C address

// Proximity threshold (0–255).
// CALIBRATED 2026-04-15 against McDonald's UK medium cup at 10 cm distance:
//   Ambient (no object)  :   5 – 16
//   Cup at exactly 10 cm :  94 – 112  (average 103)
//   Cup closer than 10 cm: 112 – 255
//
// Threshold set to 90 (slightly below the 10 cm floor of 94) to give a small
// margin for cup angle/reflectivity variation while still reliably excluding
// objects further than 10 cm.
//
// To re-calibrate: run tests/gesture_direct_test.py and note PDATA at your
// target distance, then set this to ~10% below that minimum reading.
constexpr int GESTURE_THRESHOLD = 90;

// LCD1602 with PCF8574T I2C backpack
constexpr int LCD_I2C_BUS     = 1;
constexpr int LCD_I2C_ADDRESS = 0x27;

#endif // PINCONFIG_H
