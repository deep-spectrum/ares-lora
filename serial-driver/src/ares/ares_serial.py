from ares_lora_serial_core import _SerialConfigs, _AresSerial, AresTimeout, _AresLoraConfig
from typing import Callable, overload


class LoraSerial:
    def __init__(self, port: str, response_timeout: float = 2000.0, rx_period: float = 0.1, serial_timeout: float = 0.1, start_callback: Callable[[int, int], None] | None = None):
        if not port:
            raise ValueError("Invalid port")
        configs = _SerialConfigs(
            port=port,
            response_timeout=response_timeout,
            rx_period=rx_period,
            serial_timeout=serial_timeout,
            start_callback=self._handle_start
        )

        self._start_cb = start_callback
        self._dev = _AresSerial(configs)
        self._nodes: dict[int, dict[str, int]] = {}

    def _should_event_be_dispatched(self, src: int, seq_cnt: int, packet_id: int) -> bool:
        if src not in self._nodes:
            self._nodes[src] = {"sequence": seq_cnt, "packet": packet_id, "drops": 0}
            return True

        next_seq = (self._nodes[src]["sequence"] + 1) % 256
        if next_seq != seq_cnt:
            self._nodes[src]["drops"] += 1
        self._nodes[src]["sequence"] = seq_cnt

        if self._nodes[src]["packet"] != packet_id:
            self._nodes[src]["packet"] = packet_id
            return True
        return False

    def _handle_start(self, sec: int, nsec: int, src: int, broadcast: bool, seq_cnt: int, packet_id: int):
        if self._should_event_be_dispatched(src, seq_cnt, packet_id):
            if self._start_cb is not None:
                self._start_cb(sec, nsec)
            else:
                pass

    def setting(self):
