from ares_lora_serial_core import _AresSerial
from datetime import timedelta

s = _AresSerial("/dev/ttyACM1")
s.start_driver()

s.stop_driver()
