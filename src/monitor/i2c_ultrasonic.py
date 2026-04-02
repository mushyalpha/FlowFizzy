import smbus
import time

# I2C setup
bus = smbus.SMBus(1)

# CHANGE THIS if your address is different
I2C_ADDRESS = 0x57  

def read_distance():
    try:
        # Trigger measurement (some sensors need this)
        bus.write_byte(I2C_ADDRESS, 0x01)
        time.sleep(0.1)

        # Read 2 bytes (distance)
        data = bus.read_i2c_block_data(I2C_ADDRESS, 0x00, 2)

        # Convert to distance (mm)
        distance = (data[0] << 8) + data[1]

        return distance

    except Exception as e:
        print("Error:", e)
        return None


while True:
    dist = read_distance()

    if dist is not None:
        print(f"Distance: {dist} mm ({dist/10:.1f} cm)")
    else:
        print("Sensor read failed")

    time.sleep(1)
