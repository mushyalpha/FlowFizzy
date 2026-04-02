#ifndef PINCONFIG_H
#define PINCONFIG_H

// ─── GPIO Pin Assignments (BCM numbering) ────────────────────────────────────
// HC-SR04 Ultrasonic Sensor
constexpr int TRIG_PIN = 23;   // GPIO23 — trigger output
constexpr int ECHO_PIN = 24;   // GPIO24 — echo input

// DC Water Pump (via TIP121 transistor or relay)
constexpr int PUMP_PIN = 18;   // GPIO18 (BCM)

// YF-S401 Flow Meter
// Wiring: Red→Pin2(5V)  Black→Pin9(GND)  Yellow→Pin11(GPIO17)
// Signal is open-collector; internal pull-up enabled in FlowMeter::setupGpio()
constexpr int FLOW_PIN = 17;   // GPIO17 — BCM17 — Physical Pin 11

// ─── Sensor Settings ─────────────────────────────────────────────────────────
constexpr double TARGET_DISTANCE_CM = 12.0;   // target bottle distance
constexpr double DISTANCE_TOLERANCE_CM = 1.0; // ±1 cm tolerance (11–13 cm)

// ─── Timing Settings ─────────────────────────────────────────────────────────
// NOTE: Set back to 60 for the real demo — reduced to 5 for rapid testing.
constexpr int HOLD_TIME_SECONDS = 5;           // bottle must be stable (seconds)
constexpr int LOOP_INTERVAL_MS = 1000;         // main loop interval (1 second)

// ─── Flow Settings ───────────────────────────────────────────────────────────
// YF-S401 spec: ~5880 pulses/litre  →  1000 mL / 5880 pulses ≈ 0.1701 mL/pulse
// CALIBRATED 2026-04-02: 2711 pulses measured for 200 ml actual
// → 200 / 2711 = 0.073774 ml/pulse (sensor runs ~2× datasheet rate)
constexpr double ML_PER_PULSE    = 0.073774;  // calibrated
constexpr double TARGET_VOLUME_ML = 200.0;    // dispense 200 ml per fill

// ─── GPIO Chip ───────────────────────────────────────────────────────────
// /dev/gpiochip4 for Raspberry Pi 5; use 0 for Pi 1–4.
constexpr unsigned int GPIO_CHIP_NO = 4u;  // libgpiod chip number

// ─── Sensor Timing ───────────────────────────────────────────────────────────
constexpr int SENSOR_PERIOD_MS = 200;      // 5 Hz ultrasonic sampling rate

// ─── I2C LCD (Freenove LCD1602 with PCF8574T backpack) ───────────────────────
// Physical wiring: SDA→Pin3 (GPIO2), SCL→Pin5 (GPIO3), VDD→Pin2 (5V), VSS→GND
// Run `i2cdetect -y 1` to confirm address: default = 0x27, alt (jumper) = 0x3F
constexpr int LCD_I2C_BUS     = 1;     // /dev/i2c-1
constexpr int LCD_I2C_ADDRESS = 0x27;  // PCF8574T default

#endif // PINCONFIG_H
