from ares_lora_serial_core import _AresSerial
from datetime import timedelta

s = _AresSerial("/dev/ttyACM1")
s.start_driver()
print(s.ble_state(state=0))
print(s.ble_state())
s.stop_driver()
