from ares_lora import LoraSerial, SettingId, LoraConfig, LoraLedState, BleState; s = LoraSerial('/dev/ttyACM3'); s.start_driver()
import time

def config_changed_hook(name, value):
    print(f"{name}: {value}")


def start_event_hook(delay: int):
    print(f"Start event received. Start run in {delay} seconds")


s.register_config_changed_event_hook(config_changed_hook)
s.register_ble_start_event_hook(start_event_hook)

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

print(s.ble_connection_parameters)

neighbors = {
    1: True,
    2: False,
    3: False,
    4: True,
}
s.ble_send_neighbor_data(neighbors)

while True:
    try:
        pass
    except KeyboardInterrupt:
        break

s.stop_driver()
