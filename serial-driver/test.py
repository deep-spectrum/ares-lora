from ares_lora_serial_core import _AresSerial
from datetime import timedelta

s = _AresSerial("/dev/ttyACM1")
s.start_driver()
print(s.start(10, 20, response_timeout=30.0, ack_timeout=5.0, broadcast=True))
s.stop_driver()
