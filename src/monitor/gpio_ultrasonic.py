import lgpio as GPIO
import time

TRIG = 23
ECHO = 24

# Try 0 first. If it fails, change to 4.
h = GPIO.gpiochip_open(0)

GPIO.gpio_claim_output(h, TRIG)
GPIO.gpio_claim_input(h, ECHO)

def get_distance():
    # make sure trig is low
    GPIO.gpio_write(h, TRIG, 0)
    time.sleep(0.05)

    # send 10 us pulse
    GPIO.gpio_write(h, TRIG, 1)
    time.sleep(0.00001)
    GPIO.gpio_write(h, TRIG, 0)

    pulse_start = time.time()
    timeout_start = time.time()

    # wait for echo to go high
    while GPIO.gpio_read(h, ECHO) == 0:
        pulse_start = time.time()
        if pulse_start - timeout_start > 0.03:
            return None

    pulse_end = time.time()
    timeout_start = time.time()

    # wait for echo to go low
    while GPIO.gpio_read(h, ECHO) == 1:
        pulse_end = time.time()
        if pulse_end - timeout_start > 0.03:
            return None

    pulse_duration = pulse_end - pulse_start
    distance = pulse_duration * 17150
    return round(distance, 2)

try:
    while True:
        distance = get_distance()
        if distance is None:
            print("Out of range / timeout")
        else:
            print(f"Distance: {distance:.2f} cm")
        time.sleep(1)

except KeyboardInterrupt:
    print("Stopped by user")
    GPIO.gpiochip_close(h)
