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
// pump starts.  Prevents false triggers from objects passing the sensor.
constexpr int CUP_CONFIRM_MS = 800;   // ms

// ─── Flow Settings ───────────────────────────────────────────────────────────
// YF-S401 spec: ~5880 pulses/litre  →  1000 mL / 5880 pulses ≈ 0.1701 mL/pulse
// CALIBRATED 2026-04-02: 2711 pulses measured for 200 ml actual
// → 200 / 2711 = 0.073774 ml/pulse (sensor runs ~2× datasheet spec)
constexpr double ML_PER_PULSE = 0.073774;   // mL per flow-meter pulse (calibrated)

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
// Ambient reading with no object: ~7-13.  Cup at 5-10 cm: ~15-30.
// Raise if false triggers occur; lower if cup is not detected reliably.
constexpr int GESTURE_THRESHOLD = 20;

// LCD1602 with PCF8574T I2C backpack
constexpr int LCD_I2C_BUS     = 1;
constexpr int LCD_I2C_ADDRESS = 0x27;

#endif // PINCONFIG_H
