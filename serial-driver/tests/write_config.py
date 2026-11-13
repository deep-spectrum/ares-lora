from ares_lora import LoraSerial, SettingId, LoraConfig, LoraLedState, BleState; s = LoraSerial('/dev/ttyACM3'); s.start_driver()
import time

def config_changed_hook(name, value):
    print(f"{name}: {value}")


s.register_config_changed_event_hook(config_changed_hook)

if s.ble_state() == BleState.OFF:
    s.ble_state(BleState.ON)

subs = {'foo': False}
while not all(subs.values()):
    subs = s.wait_subscription_change_event()

d = {
    "bandwidth": 100e6,
    "duration": 30,
    "center_freq": 2.45e9,
    "ref_level": -20.0,
    "description": "This is a test from Python."
}
print(s.node_configs)

s.node_configs = d

time.sleep(5)
s.stop_driver()
