from ares_lora_serial_core import _AresSerial
from datetime import timedelta

s = _AresSerial("/dev/ttyACM1")
s.start_driver()
print(s.log(message="This is a very long message. Did you know C++ is a horrible language. It should not be used in the future. Anything done over serial will be done in C instead because things are so much easier to keep track of.", response_timeout=30.0, destination=1, retries=3))
s.stop_driver()
