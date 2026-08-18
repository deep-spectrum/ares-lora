from ares_lora_serial_core import _AresSerial
from datetime import timedelta

s = _AresSerial("/dev/ttyACM1")
s.start_driver()
print(s.node_config(response_timeout=20.0, bandwidth=160e6, center_freq=2.45e9, duration=30, ref_level=-20.0))
s.stop_driver()
