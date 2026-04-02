import lgpio as GPIO
import time

# -----------------------
# Pin configuration
# -----------------------
TRIG = 23   # GPIO23
ECHO = 24   # GPIO24

# -----------------------
# Open GPIO chip
# -----------------------
h = GPIO.gpiochip_open(0)

GPIO.gpio_claim_output(h, TRIG)
GPIO.gpio_claim_input(h, ECHO)

# -----------------------
# Function to get distance
# -----------------------
def get_distance():
    # Ensure TRIG is LOW
    GPIO.gpio_write(h, TRIG, 0)
    time.sleep(0.05)

    # Send 10us pulse
    GPIO.gpio_write(h, TRIG, 1)
    time.sleep(0.00001)
    GPIO.gpio_write(h, TRIG, 0)

    # Wait for echo start
    start_time = time.time()
    timeout = start_time + 0.02

    while GPIO.gpio_read(h, ECHO) == 0:
        pulse_start = time.time()
        if pulse_start > timeout:
            return None  # timeout safety

    # Wait for echo end
    timeout = time.time() + 0.02
    while GPIO.gpio_read(h, ECHO) == 1:
        pulse_end = time.time()
        if pulse_end > timeout:
            return None  # timeout safety

    # Calculate distance
    pulse_duration = pulse_end - pulse_start
    distance = pulse_duration * 17150  # cm
    distance = round(distance, 2)

    return distance


# -----------------------
# Main loop
# -----------------------
if __name__ == "__main__":
    try:
        while True:
            dist = get_distance()

            if dist is not None:
                print(f"Measured Distance = {dist:.2f} cm")
            else:
                print("Out of range / timeout")

            time.sleep(1)

    except KeyboardInterrupt:
        print("Measurement stopped by User")
        GPIO.gpiochip_close(h)
