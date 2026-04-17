from ares_lora_serial_core import _SerialConfigs, _AresSerial, AresTimeout, _AresLoraConfig
from typing import Callable
from enum import IntEnum
from dataclasses import dataclass, asdict
import functools
from .errno import strerror
import logging


logger = logging.getLogger("ares_lora")


class LoraException(Exception):
    def __init__(self, code: int):
        super().__init__(strerror(code))


class SettingId(IntEnum):
    ID = 0
    WAIT_USB_HOST = 1
    PANID = 2
    REPETITION_CNT = 3


class LoraBandwidth(IntEnum):
    BW_125_KHZ = 0
    BW_250_KHZ = 1
    BW_500_KHZ = 2


class LoraSpreadingFactor(IntEnum):
    SF_6 = 6
    SF_7 = 7
    SF_8 = 8
    SF_9 = 9
    SF_10 = 10
    SF_11 = 11
    SF_12 = 12


class LoraCodingRate(IntEnum):
    CR_4_5 = 1
    CR_4_6 = 2
    CR_4_7 = 3
    CR_4_8 = 4


@dataclass
class LoraConfig:
    frequency: int = 915000000
    bandwidth: LoraBandwidth = LoraBandwidth.BW_125_KHZ
    datarate: LoraSpreadingFactor = LoraSpreadingFactor.SF_12
    coding_rate: LoraCodingRate = LoraCodingRate.CR_4_5
    preamble_length: int = 8
    tx_power: int = 10


class LoraLedState(IntEnum):
    OFF = 0
    ON = 1
    BLINK = 2
    FETCH = 3


@dataclass
class LoraSerialConfig:
    port: str = ""
    response_timeout: float = 2.0
    rx_period: float = 0.1
    serial_timeout: float = 0.1
    start_callback: Callable[[int, int], None] | None = None
    heartbeat_callback: Callable[[int, bool, bool], None] | None = None
    claim_callback: Callable[[int], None] | None = None
    master: bool = False
    log_callback: Callable[[int, str], None] = None


@dataclass
class LogMessage:
    msg_id: int
    last_part: int
    total_parts: int
    msg: str


def lora_serial_command(func):
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except AresTimeout as e:
            raise TimeoutError(str(e))

    return wrapper


class LoraSerial:
    def __init__(self, config: LoraSerialConfig = LoraSerialConfig()):
        if not config.port:
            raise ValueError("Invalid port")
        configs = _SerialConfigs(
            port=config.port,
            response_timeout=config.response_timeout,
            rx_period=config.rx_period,
            serial_timeout=config.serial_timeout,
            master=config.master,
            start_callback=self._handle_start,
            heartbeat_callback=self._handle_heartbeat,
            claim_callback=self._handle_claim,
            log_callback=self._handle_log,
        )

        self._start_cb = config.start_callback
        self._heartbeat_cb = config.heartbeat_callback
        self._claim_cb = config.claim_callback
        self._log_cb = config.log_callback
        self._dev = _AresSerial(configs)
        self._nodes: dict[int, dict[str, int]] = {}
        self._log_msg: dict[int, LogMessage] = {}

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
                logger.info(f"Received start message (sec: {sec}, nsec: {nsec}, src: {src}, "
                            f"broadcast: {broadcast}, sequence count: {seq_cnt}, packet id: {packet_id})")

    def _handle_heartbeat(self, src_id: int, ready: bool, broadcast: bool):
        pass

    def _handle_claim(self, src_id: int):
        pass

    def _handle_log(self, src_id: int, chunk: int, num_chunks: int, msg: str):
        # TODO: Log message ID
        if src_id not in self._log_msg:
            self._log_msg[src_id] = LogMessage(0, chunk, num_chunks, msg)
        # TODO: Check if message IDs match, if not overwrite the old message

        elif self._log_msg[src_id].last_part != chunk and (self._log_msg[src_id].last_part + 1) == chunk:
            print(msg)
            self._log_msg[src_id].msg = f"{self._log_msg[src_id].msg}{msg}"
            self._log_msg[src_id].last_part = chunk

        if self._log_msg[src_id].last_part == self._log_msg[src_id].total_parts:
            if self._log_cb is not None:
                self._log_cb(src_id, msg)
            print(self._log_msg[src_id].msg)

    @staticmethod
    def _check_ret_code(code: int | tuple[int, ...]):
        if isinstance(code, int):
            if code != 0:
                raise LoraException(code)
            return
        for c in code:
            if c != 0:
                raise LoraException(c)

    @lora_serial_command
    def setting(self, setting_id: SettingId, value: int | None = None) -> int | None:
        if value is None:
            ret, err_code = self._dev.setting_get(setting_id.value)
            self._check_ret_code(err_code)
            return ret
        err_code = self._dev.setting_set(setting_id.value, value)
        self._check_ret_code(err_code)
        return None

    @lora_serial_command
    def start(self, sec: int, nsec: int, timeout: float = 20.0, broadcast: bool = True, destination_id: int | None = None):
        if not broadcast and (destination_id is None or destination_id <= 0):
            raise ValueError("Direct messages must have a valid destination specified")
        if sec < 0:
            raise ValueError("Time must be positive")
        if destination_id is None:
            destination_id = 0
        prev_timeout = self._dev.get_response_timeout()
        self._dev.set_response_timeout(timeout)
        try:
            ret = self._dev.start(sec, nsec, destination_id, broadcast)
        except Exception:
            self._dev.set_response_timeout(prev_timeout)
            raise
        else:
            self._dev.set_response_timeout(prev_timeout)
        self._check_ret_code(ret)

    @lora_serial_command
    def lora_config(self, config: LoraConfig):
        args = asdict(config)
        for key in args.keys():
            if not isinstance(args[key], int):
                args[key] = args[key].value
        configs_ = _AresLoraConfig(**args)
        ret = self._dev.lora_config(configs_)
        self._check_ret_code(ret)

    @lora_serial_command
    def led(self, state: LoraLedState = LoraLedState.FETCH) -> LoraLedState | None:
        ret, err_code = self._dev.led(state.value)
        self._check_ret_code(err_code)
        if state == LoraLedState.FETCH:
            return LoraLedState(ret)
        return None

    @lora_serial_command
    def send_heartbeat(self, ready: bool, strobe_count: int = 3, timeout: float = 20.0) -> None:
        if strobe_count <= 0:
            raise ValueError("strobe_count must be a positive, non-zero integer")
        prev_timeout = self._dev.get_response_timeout()
        self._dev.set_response_timeout(timeout)
        try:
            code = self._dev.send_heartbeat(ready, strobe_count)
        except Exception:
            self._dev.set_response_timeout(prev_timeout)
            raise
        else:
            self._dev.set_response_timeout(prev_timeout)
        self._check_ret_code(code)

    @lora_serial_command
    def send_log(self, log_msg: str, broadcast: bool = False, dst_id: int = 0, strobe_count: int = 3, timeout: float = 15.0):
        if strobe_count <= 0:
            raise ValueError("strobe_count must be a positive, non-zero integer")
        prev_timeout = self._dev.get_response_timeout()
        self._dev.set_response_timeout(timeout)
        try:
            codes = self._dev.send_log(log_msg, broadcast, strobe_count, dst_id)
        except Exception:
            self._dev.set_response_timeout(prev_timeout)
            raise
        else:
            self._dev.set_response_timeout(prev_timeout)
        self._check_ret_code(codes)

    @lora_serial_command
    def version(self) -> tuple[tuple[int, int, int], tuple[int, int, int], tuple[int, int, int]]:
        return self._dev.version()

    def start_driver(self):
        self._dev.start_driver()

    def stop_driver(self):
        self._dev.stop_driver()

    def __enter__(self):
        self.start_driver()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop_driver()
