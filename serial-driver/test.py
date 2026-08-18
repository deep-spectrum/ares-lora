from ares_lora_serial_core import _AresSerial
from datetime import timedelta
import time

s = _AresSerial("/dev/ttyACM1")
s.start_driver()
print(s.ble_disconnect())
print(s.ble_state(state=1))
#print(s.ble_disconnect())
time.sleep(10)
#print(s.ble_disconnect())
print(s.ble_state(state=0))
s.stop_driver()
