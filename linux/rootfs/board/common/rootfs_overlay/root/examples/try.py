
import sys
import time

def read_discovery():
        fp1 = open("/sys/soo/soolink/discovery/stream_count", "r")
        value = fp1.read();
        return value
        fp1.close()

while True:
        val = read_discovery()
        print("value " + val)
        time.sleep(1)
