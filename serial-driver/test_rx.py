from ares_lora_serial_core import _AresSerial

s = _AresSerial("/dev/ttyACM3")
s.start_driver()
s.ready = True

while True:
    pass
