from ares_lora import LoraSerial, SettingId, LoraConfig, LoraLedState, BleState, AresBleContextManager; s = LoraSerial('/dev/ttyACM3'); s.start_driver()
import time

def config_changed_hook(name, value):
    print(f"{name}: {value}")


def start_event_hook(delay: int):
    print(f"Start event received. Start run in {delay} seconds")


if s.setting(SettingId.ID) == 0:
    s.setting(SettingId.ID, value=70)


s.register_config_changed_event_hook(config_changed_hook)
s.register_ble_start_event_hook(start_event_hook)

with AresBleContextManager(s) as ctx:
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

    time.sleep(30)

s.stop_driver()
