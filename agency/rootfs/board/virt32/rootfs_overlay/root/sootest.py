# Read sysfs entry related to the discovery test code
# and display a circular counter (0-9)

import sys
import time
from sense_emu import SenseHat

sense = SenseHat()

def read_discovery():
        fp1 = open("/sys/soo/soolink/discovery/stream_count", "r")
        value = fp1.read();
        return value
        fp1.close()

while True:
        val = read_discovery()
        print("value " + val)
        count = int(val) % 10
        sense.show_letter(str(count))
        time.sleep(1)
