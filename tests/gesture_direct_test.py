#!/usr/bin/env python3
"""
APDS9960 — Standalone Gesture Isolation Test
=============================================
Directly configures the sensor over I2C and prints raw FIFO data
plus the decoded gesture.  Run independently of the main application.

Usage:
    sudo python3 tests/gesture_direct_test.py

Controls:
    Ctrl+C to quit.
"""

import time
import sys

try:
    import smbus2 as smbus_mod
    SMBus = smbus_mod.SMBus
except ImportError:
    try:
        import smbus as smbus_mod
        SMBus = smbus_mod.SMBus
    except ImportError:
        sys.exit("ERROR: install python3-smbus2 or python3-smbus first:\n"
                 "  sudo apt install python3-smbus2")

# ── Constants ─────────────────────────────────────────────────────────────────
I2C_BUS      = 1
APDS_ADDR    = 0x39

# Registers
REG_ENABLE   = 0x80
REG_PPULSE   = 0x8E
REG_CONTROL  = 0x8F
REG_CONFIG2  = 0x90
REG_ID       = 0x92
REG_GPENTH   = 0xA0
REG_GEXTH    = 0xA1
REG_GCONFIG1 = 0xA2
REG_GCONFIG2 = 0xA3
REG_GPULSE   = 0xA6
REG_GCONFIG3 = 0xAA
REG_GCONFIG4 = 0xAB
REG_GFLVL    = 0xAE
REG_GSTATUS  = 0xAF
REG_GFIFO_U  = 0xFC

# ENABLE register bits
PON  = 0x01   # Power on
AEN  = 0x02   # ALS enable
PEN  = 0x04   # Proximity enable
GEN  = 0x40   # Gesture enable


# ── I2C helpers ───────────────────────────────────────────────────────────────
def wr(bus, reg, val):
    bus.write_byte_data(APDS_ADDR, reg, val)

def rd(bus, reg):
    return bus.read_byte_data(APDS_ADDR, reg)

def rd_block(bus, reg, length):
    return bus.read_i2c_block_data(APDS_ADDR, reg, length)


# ── Sensor initialisation ─────────────────────────────────────────────────────
def init_sensor(bus):
    # Full power-off then on
    wr(bus, REG_ENABLE, 0x00)
    time.sleep(0.05)
    wr(bus, REG_ENABLE, PON)
    time.sleep(0.01)

    # Proximity pulse: 8 µs pulse length, 10 pulses
    wr(bus, REG_PPULSE, 0x89)

    # Gesture pulse: 32 µs pulse length, 10 pulses
    wr(bus, REG_GPULSE, 0xC9)

    # PGAIN = 4×, AGAIN = 4×, LDRIVE = 100 mA
    wr(bus, REG_CONTROL, 0x06)

    # LED boost = 300 %
    wr(bus, REG_CONFIG2, 0x01)

    # Gesture entry / exit thresholds
    wr(bus, REG_GPENTH, 5)    # enter at PDATA > 5
    wr(bus, REG_GEXTH,  30)   # exit  at PDATA < 30

    # GCONFIG1: FIFO interrupt after 4 datasets
    wr(bus, REG_GCONFIG1, 0x40)

    # GCONFIG2: GGAIN = 4×, GLDRIVE = 100 mA, GWTIME = 2.8 ms
    wr(bus, REG_GCONFIG2, 0x66)

    # GCONFIG3: all 4 photodiode pairs active
    wr(bus, REG_GCONFIG3, 0x00)

    # GCONFIG4: gesture mode ON, interrupt enable
    wr(bus, REG_GCONFIG4, 0x02)

    # Enable: Power ON + ALS + Proximity + Gesture
    wr(bus, REG_ENABLE, PON | AEN | PEN | GEN)
    time.sleep(0.5)


# ── Gesture decode ─────────────────────────────────────────────────────────────
def decode_gesture(datasets):
    """
    datasets: list of (U, D, L, R) tuples from the FIFO.
    Returns 'UP', 'DOWN', 'LEFT', 'RIGHT', or None.
    """
    if len(datasets) < 4:
        return None

    ud_count = 0
    lr_count = 0

    for u, d, l, r in datasets:
        total = u + d + l + r
        if total < 10:
            continue  # too dim — sensor noise, skip
        ud = int(u) - int(d)
        lr = int(l) - int(r)
        if abs(ud) > 13:
            ud_count += 1 if ud > 0 else -1
        if abs(lr) > 13:
            lr_count += 1 if lr > 0 else -1

    if ud_count == 0 and lr_count == 0:
        return None

    if abs(ud_count) > abs(lr_count):
        return "UP"   if ud_count > 0 else "DOWN"
    else:
        return "RIGHT" if lr_count > 0 else "LEFT"


# ── Main loop ─────────────────────────────────────────────────────────────────
def main():
    print("APDS9960 Gesture Isolation Test")
    print("================================")

    bus = SMBus(I2C_BUS)

    dev_id = rd(bus, REG_ID)
    print(f"Device ID : 0x{dev_id:02X}  (expected 0xAB or 0xAA)")
    if dev_id not in (0xAB, 0xAA):
        print("WARNING: Unexpected device ID — check wiring / I2C address")

    init_sensor(bus)
    print("Sensor initialised in gesture mode.")
    print("\nWave your hand slowly (~5 cm) over the sensor.")
    print("Press Ctrl+C to exit.\n")

    while True:
        gstatus = rd(bus, REG_GSTATUS)
        gvalid  = gstatus & 0x01   # GVALID bit
        gfov    = (gstatus >> 1) & 0x01  # GFOV  bit (overflow)

        if gfov:
            print("[WARN] FIFO overflow — clearing")
            wr(bus, REG_GCONFIG4, rd(bus, REG_GCONFIG4) | 0x04)  # GFIFO_CLR

        if gvalid:
            fifo_level = rd(bus, REG_GFLVL)
            if fifo_level == 0:
                time.sleep(0.01)
                continue

            # smbus2 limits block reads to 32 bytes.
            # We must read in chunks if the FIFO has more than 8 datasets (8 * 4 = 32).
            raw = []
            bytes_to_read = fifo_level * 4
            while len(raw) < bytes_to_read:
                chunk = min(32, bytes_to_read - len(raw))
                raw.extend(rd_block(bus, REG_GFIFO_U, chunk))
            datasets = [
                (raw[i*4], raw[i*4+1], raw[i*4+2], raw[i*4+3])
                for i in range(fifo_level)
            ]

            # Print raw values
            print(f"FIFO [{fifo_level:2d} sets] ", end="")
            for u, d, l, r in datasets:
                print(f"U{u:3d} D{d:3d} L{l:3d} R{r:3d} | ", end="")
            print()

            gesture = decode_gesture(datasets)
            if gesture:
                print(f"\n  >>>  {gesture}  <<<\n")

        time.sleep(0.02)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nExiting.")
