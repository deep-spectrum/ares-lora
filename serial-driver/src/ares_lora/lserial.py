from ares_lora_serial_core import _SerialConfigs, _AresSerial, AresTimeout, _AresLoraConfig, AresThreadTerminate
from typing import Callable
from enum import IntEnum
from dataclasses import dataclass, asdict
import functools
from .errno import strerror
import logging
from .utils import check_serial_port
from threading import Lock
import copy
import ctypes
from concurrent.futures import ThreadPoolExecutor, Future

logger = logging.getLogger("ares_lora")


class LoraException(Exception):
    """Exception class for LoRa related exceptions."""

    def __init__(self, code: int):
        super().__init__(strerror(code))


class SettingId(IntEnum):
    """Firmware settings for LoRa.

    Attributes:
        ID: The ID of the node. This should be unique to each node. [1, 65535].
        WAIT_USB_HOST: Flag for telling the firmware to wait for a USB connection. [0,1].
        PANID: The personal area network ID. [0, 65535].
        REPETITION_CNT: The default number of times a LoRa message is transmitted. [1, 4294967295].
    """

    ID = 0
    WAIT_USB_HOST = 1
    PANID = 2
    REPETITION_CNT = 3


class LoraBandwidth(IntEnum):
    """LoRa signal bandwidth.

    This enumeration defines the bandwidth of a LoRa signal.

    The bandwidth determines how much spectrum is used to transmit data.
    Wider bandwidths enable higher data rates but typically reduce sensitivity and range.

    Attributes:
        BW_125_KHZ: 125 kHz.
        BW_250_KHZ: 250 kHz.
        BW_500_KHZ: 500 kHz.
    """

    BW_125_KHZ = 0
    BW_250_KHZ = 1
    BW_500_KHZ = 2


class LoraSpreadingFactor(IntEnum):
    """LoRa data rate.

    This enumeration represents the data rate of a LoRa signal, expressed as a Spreading Factor (SF).

    The Spreading Factor determines how many chirps are used to encode each symbol (2^SF chips per symbol).
    Higher values result in lower data rates but increased range and robustness.

    Attributes:
        SF_6: Spreading factor 6 (fastest, shortest range).
        SF_7: Spreading factor 7.
        SF_8: Spreading factor 8.
        SF_9: Spreading factor 9.
        SF_10: Spreading factor 10.
        SF_11: Spreading factor 11.
        SF_12: Spreading factor 12 (slowest, longest range).
    """

    SF_6 = 6
    SF_7 = 7
    SF_8 = 8
    SF_9 = 9
    SF_10 = 10
    SF_11 = 11
    SF_12 = 12


class LoraCodingRate(IntEnum):
    """LoRa coding rate.

    This enumeration defines the LoRa coding rate, used for forward error correction (FEC).

    The coding rate is expressed as 4/x, where a lower denominator (e.g., 4/5) means less redundancy,
    resulting in a higher data rate but reduced robustness. Higher redundancy (e.g., 4/8) improves error
    tolerance at the cost of data rate.

    Attributes:
        CR_4_5: Coding rate 4/5 (4 information bits, 1 error correction bit).
        CR_4_6: Coding rate 4/6 (4 information bits, 2 error correction bits).
        CR_4_7: Coding rate 4/7 (4 information bits, 3 error correction bits).
        CR_4_8: Coding rate 4/8 (4 information bits, 4 error correction bits).
    """

    CR_4_5 = 1
    CR_4_6 = 2
    CR_4_7 = 3
    CR_4_8 = 4


@dataclass
class LoraConfig:
    """Configurations for the LoRa modem.

    Attributes:
        frequency: Frequency in Hz to use for transceiving. Default is 915 MHz.
        bandwidth: The bandwidth to use for transceiving. Default is 125 kHz.
        datarate: The data-rate to use for transceiving. Default is SF_12.
        coding_rate: The coding rate to use for transceiving. Default is CR_4_5.
        preamble_length: Length of the preamble. Default is 8.
        tx_power: TX-power in dBm to use for transmission. Default is 10 dBm.
    """

    frequency: int = 915000000
    bandwidth: LoraBandwidth = LoraBandwidth.BW_125_KHZ
    datarate: LoraSpreadingFactor = LoraSpreadingFactor.SF_12
    coding_rate: LoraCodingRate = LoraCodingRate.CR_4_5
    preamble_length: int = 8
    tx_power: int = 10


class LoraLedState(IntEnum):
    """Different states the LED can be in (except for fetch).

    Attributes:
        OFF: LED is turned off.
        ON: LED is solid on.
        BLINK: LED is blinking at 1 Hz.
        FADE: LED is fading on and off.
        FETCH: Fetch the current LED state from the firmware.
    """
    OFF = 0
    ON = 1
    BLINK = 2
    FADE = 3
    FETCH = 4


@dataclass
class LoraSerialConfig:
    """Configurations for the LoRa serial driver.

    Attributes:
        port: The serial port to connect to.
        response_timeout: The amount of time (in seconds) to wait for a response from the firmware.
        rx_period: How often (in seconds) the serial driver polls the serial receive buffer.
        serial_timeout: The serial RX timeout (in seconds).
        start_callback: Event handler for start events. Signature: [seconds: int, nsecs: int] -> None
        heartbeat_callback: Event handler for heartbeat events. Signature: [source_id: int, read: bool] -> None
        claim_callback: Event handler for claim master events. Signature: [source_id: int] -> None
        master: Designate the connected node as the master node.
        log_callback: Event handler for log events. Signature: [source_id: int, msg: str] -> None
    """
    port: str = ""
    response_timeout: float = 2.0
    rx_period: float = 0.1
    serial_timeout: float = 0.1
    start_callback: Callable[[int, int], None] | None = None
    heartbeat_callback: Callable[[int, bool], None] | None = None
    claim_callback: Callable[[int], None] | None = None
    master: bool = False
    log_callback: Callable[[int, str], None] = None


@dataclass
class LogMessage:
    msg_id: int
    last_part: int
    total_parts: int
    msg: str
    transmitted: bool = False


def lora_serial_command(func):
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except AresTimeout as e:
            raise TimeoutError(str(e))

    return wrapper


class LoraSerial:
    """LoRa serial driver python implementation. Works only on Linux."""

    def __init__(self, config: LoraSerialConfig = LoraSerialConfig()):
        """Initializes the LoRa driver.

        Args:
            config: The configurations for the LoRa driver.

        Raises:
            ValueError: Empty port configuration.
            IOError: Port not found.
        """
        if not config.port:
            raise ValueError("Invalid port")
        if not check_serial_port(config.port):
            raise IOError(f"Cannot open port {config.port}: Does not exist")
        configs = _SerialConfigs(
            port=config.port,
            response_timeout=config.response_timeout,
            rx_period=config.rx_period,
            serial_timeout=config.serial_timeout,
            master=config.master,
        )

        self._start_cb = config.start_callback
        self._heartbeat_cb = config.heartbeat_callback
        self._claim_cb = config.claim_callback
        self._log_cb = config.log_callback
        self._dev = _AresSerial(configs)
        self._nodes: dict[int, int] = {}
        self._log_msg: dict[int, LogMessage] = {}

        self._rx_stats: dict[int, int] = {}
        self._rx_stats_lock = Lock()

        self._tx_stats: int = 0
        self._tx_stats_lock = Lock()

        self._logger = logger

        self._event_worker_pool = ThreadPoolExecutor(max_workers=6)
        self._start_future: Future[None] | None = None
        self._heartbeat_future: Future[None] | None = None
        self._claim_future: Future[None] | None = None
        self._log_future: Future[None] | None = None
        self._pkt_rx_future: Future[None] | None = None
        self._pkt_tx_future: Future[None] | None = None

    def _should_event_be_dispatched(self, src: int, packet_id: int) -> bool:
        if src not in self._nodes:
            self._nodes[src] = packet_id
            return True

        if self._nodes[src] != packet_id:
            self._nodes[src] = packet_id
            return True
        return False

    def _start_event_handle(self):
        while True:
            try:
                sec, usec, src, broadcast, seq_cnt, packet_id = self._dev.wait_start_event()
            except AresThreadTerminate:
                break

            if self._should_event_be_dispatched(src, packet_id):
                logger.info(f"Received start message (sec: {sec}, usec: {usec}, src: {src}, "
                            f"broadcast: {broadcast}, sequence count: {seq_cnt}, packet id: {packet_id})")
                if self._start_cb is not None:
                    self._start_cb(sec, usec)

    def _heartbeat_event_handle(self):
        while True:
            try:
                src_id, ready, broadcast = self._dev.wait_heartbeat_event()
            except AresThreadTerminate:
                break

            logger.info(f"Received heartbeat message: (source: {src_id}, ready: {ready}, broadcasted: {broadcast}")
            if self._heartbeat_cb is not None:
                self._heartbeat_cb(src_id, ready)

    def _claim_event_handle(self):
        while True:
            try:
                src_id = self._dev.wait_claim_event()
            except AresThreadTerminate:
                break
            if self._claim_cb is not None:
                self._claim_cb(src_id)

    def _log_event_handle(self):
        while True:
            try:
                src_id, log_id, chunk, num_chunks, msg = self._dev.wait_log_event()
            except AresThreadTerminate:
                break

            if src_id not in self._log_msg:
                self._log_msg[src_id] = LogMessage(log_id, chunk, num_chunks, msg)
            elif log_id != self._log_msg[src_id].msg_id:
                self._log_msg[src_id] = LogMessage(log_id, chunk, num_chunks, msg)
            elif self._log_msg[src_id].last_part != chunk and (self._log_msg[src_id].last_part + 1) == chunk:
                self._log_msg[src_id].msg = f"{self._log_msg[src_id].msg}{msg}"
                self._log_msg[src_id].last_part = chunk

            if (self._log_msg[src_id].last_part == self._log_msg[src_id].total_parts and
                    not self._log_msg[src_id].transmitted):
                logger.info(f"Received log message: {self._log_msg[src_id].msg}")
                self._log_msg[src_id].transmitted = True
                if self._log_cb is not None:
                    self._log_cb(src_id, self._log_msg[src_id].msg)

    def _pkt_rx_event_handle(self):
        while True:
            try:
                seq_cnt, packet_id, source_id = self._dev.wait_packet_rx_event()
            except AresThreadTerminate:
                break

            with self._rx_stats_lock:
                if source_id not in self._rx_stats:
                    self._rx_stats[source_id] = 1
                else:
                    self._rx_stats[source_id] += 1

    def _pkt_tx_done_event_handle(self):
        while True:
            try:
                count = self._dev.wait_packet_tx_done_event()
            except AresThreadTerminate:
                break

            with self._tx_stats_lock:
                self._tx_stats += count

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
        """Set or retrieve a LoRa firmware setting.

        Args:
            setting_id: The setting to read or write to.
            value: The new value of the setting. If None, reads the specified setting.

        Returns:
            If writing a setting, None. If reading a setting, the value of the setting.

        Raises:
            TimeoutError: No response from the firmware within the configured timeout.
            LoraException: Firmware responded with an error code.
        """
        if value is None:
            ret, err_code = self._dev.setting_get(setting_id.value)
            self._check_ret_code(err_code)
            return ret
        err_code = self._dev.setting_set(setting_id.value, value)
        self._check_ret_code(err_code)
        return None

    @lora_serial_command
    def start(self, sec: int, usec: int, timeout: float = 20.0, broadcast: bool = True,
              destination_id: int | None = None) -> None:
        """Send start time over LoRa

        Args:
            sec: The seconds part of the time to start.
            usec: The microseconds part of the time to start.
            timeout: The timeout of the transmission.
            broadcast: Broadcast the message to all the nodes.
            destination_id: The destination node if not broadcasting. This field is ignored if broadcasting.

        Raises:
            ValueError: The destination ID is invalid.
            ValueError: The start time is invalid.
            TimeoutError: No response from the firmware within the timeout.
            LoraException: Firmware responded with an error code.
        """
        if not broadcast and (destination_id is None or destination_id <= 0):
            raise ValueError("Direct messages must have a valid destination specified")
        if sec < 0 or usec < 0:
            raise ValueError("Time must be positive")
        if destination_id is None:
            destination_id = 0
        prev_timeout = self._dev.get_response_timeout()
        self._dev.set_response_timeout(timeout)
        try:
            ret = self._dev.start(sec, usec, destination_id, broadcast)
        except Exception:
            self._dev.set_response_timeout(prev_timeout)
            raise
        else:
            self._dev.set_response_timeout(prev_timeout)
        self._check_ret_code(ret)

    @lora_serial_command
    def lora_config(self, config: LoraConfig):
        """Configure the LoRa modem.

        Args:
            config: The LoRa modem configurations.

        Raises:
            TimeoutError: No response from the firmware within the configured timeout.
            LoraException: Firmware responded with an error code.
        """
        args = asdict(config)
        for key in args.keys():
            if not isinstance(args[key], int):
                args[key] = args[key].value
        configs_ = _AresLoraConfig(**args)
        ret = self._dev.lora_config(configs_)
        self._check_ret_code(ret)

    @lora_serial_command
    def led(self, led_id: int, state: LoraLedState = LoraLedState.FETCH) -> LoraLedState | None:
        """Set or retrieve the state of the LED.

        Args:
            led_id: The ID/number of the LED to fetch/set the state of.
            state: The new state of the LED. If set to LoraLedState.FETCH, then retrieves the current state of the
                   LED. (Default: LoraLedState.FETCH)

        Returns:
            The current LED state if state is LoraLedState.FETCH. None otherwise.

        Raises:
            TimeoutError: No response from the firmware within the configured timeout.
            LoraException: Firmware responded with an error code.
        """
        if led_id > ctypes.c_uint8(-1).value:
            raise ValueError(f"led_id is {led_id}. Valid range: [0, {ctypes.c_uint8(-1).value}]")
        ret, err_code = self._dev.led(led_id, state.value)
        self._check_ret_code(err_code)
        if state == LoraLedState.FETCH:
            return LoraLedState(ret)
        return None

    @lora_serial_command
    def send_heartbeat(self, ready: bool, strobe_count: int = 3, timeout: float = 20.0) -> None:
        """Send a heartbeat over LoRa.

        Args:
            ready: Flag indicating that the system is ready to start collecting data.
            strobe_count: The number of times to transmit the heartbeat.
            timeout: The timeout of the transmission.

        Raises:
            ValueError: The strobe count is invalid.
            TimeoutError: No response from the firmware within the configured timeout.
            LoraException: Firmware responded with an error code.
        """
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
    def send_log(self, log_msg: str, broadcast: bool = False, dst_id: int | None = None, strobe_count: int = 3,
                 timeout: float = 15.0):
        """Send a log message over LoRa.

        Args:
            log_msg: The log message to send over LoRa.
            broadcast: Flag indicating if the message should be broadcasted to all nodes on the network.
            dst_id: The destination for the log message. Ignored if the broadcast flag is set.
            strobe_count: The number of times to send the broadcast message. The number of attempts per chunk if a
                          directed message.
            timeout: The timeout per a transmission.

        Raises:
            ValueError: The strobe count is invalid.
            TimeoutError: No response from the firmware within the timeout.
            LoraException: Firmware responded with an error code.

        Notes:
            - If the message is chunked, then timeout is the timeout for each chunk (Not the timeout for all the
              chunks to be transmitted in).
            - If broadcast is set to `False` and the destination is `None`, then the destination will be set to the
              master node. If the master node has not been claimed, then the broadcast flag will be overridden to
              be `True`.
        """
        if strobe_count <= 0:
            raise ValueError("strobe_count must be a positive, non-zero integer")
        if dst_id is None:
            dst_id = 0
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
        """Retrieves all the firmware version information.

        Returns:
            A tuple of versions. The first tuple is the application version, the second tuple is the ncs version, and the third tuple is the kernel version.

        Raises:
            TimeoutError: No response from the firmware within the configured timeout.
            LoraException: Firmware responded with an error code.

        Notes:
            A version tuple is as follows: (major, minor, patch).
        """
        return self._dev.version()

    def register_logger(self, logger_redirect: logging.Logger | None = logger):
        """Register a logger with the core module.

        Args:
            logger_redirect: The logger to register with the core. If `None`, then unregister and use the core module logger.
        """
        if logger_redirect is None:
            self._dev.register_logger_callbacks(None, None, None, None, None, None, None)
        self._logger = logger_redirect
        self._dev.register_logger_callbacks(self._debug, self._info, self._warning, self._error, self._critical,
                                            self._get_level, self._set_level)

    def set_logging_level(self, level: int):
        """Set the logging level of the LoRa driver core library.

        Args:
            level: The new logging level of the core library.

        Raises:
            ValueError: If the logging level is invalid.

        Notes:
            This is compatible with the logging levels found in the python logging module.

            - `10`: DEBUG
            - `20`: INFO
            - `30`: WARNING
            - `40`: ERROR
            - `50`: CRITICAL
            - `60`: OFF
        """
        self._dev.set_logging_level(level)

    def get_logging_level(self) -> int:
        """Retrieve the current logging level of the core logger.

        Returns:
            The logging level.

        Notes:
            This is compatible with the logging levels found in the python logging module.

            - `10`: DEBUG
            - `20`: INFO
            - `30`: WARNING
            - `40`: ERROR
            - `50`: CRITICAL
            - `60`: OFF
        """
        return self._dev.get_log_level()

    def _debug(self, msg: str):
        self._logger.debug(msg)

    def _info(self, msg: str):
        self._logger.info(msg)

    def _warning(self, msg: str):
        self._logger.warning(msg)

    def _error(self, msg: str):
        self._logger.error(msg)

    def _critical(self, msg: str):
        self._logger.critical(msg)

    def _get_level(self):
        return self._logger.level

    def _set_level(self, level: int):
        self._logger.setLevel(level)

    def start_driver(self):
        """Starts execution of the LoRa driver."""
        self._dev.start_driver()
        self._start_future = self._event_worker_pool.submit(self._start_event_handle)
        self._heartbeat_future = self._event_worker_pool.submit(self._heartbeat_event_handle)
        self._claim_future = self._event_worker_pool.submit(self._claim_event_handle)
        self._log_future = self._event_worker_pool.submit(self._log_event_handle)
        self._pkt_rx_future = self._event_worker_pool.submit(self._pkt_rx_event_handle)
        self._pkt_tx_future = self._event_worker_pool.submit(self._pkt_tx_done_event_handle)

    @staticmethod
    def _check_future(future: Future[None] | None, timeout: float):
        if future is not None:
            future.result(timeout)

    def stop_driver(self):
        """Stops execution of the LoRa driver."""
        self._dev.stop_driver()
        self._check_future(self._start_future, 0.1)
        self._check_future(self._heartbeat_future, 0.1)
        self._check_future(self._claim_future, 0.1)
        self._check_future(self._log_future, 0.1)
        self._check_future(self._pkt_rx_future, 0.1)
        self._check_future(self._pkt_tx_future, 0.1)

    def __enter__(self):
        self.start_driver()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop_driver()

    def __del__(self):
        self.stop_driver()

    @property
    def reception_count(self) -> dict[int, int]:
        """The number of packets received and recovered from LoRa.

        Returns:
            A dictionary of node IDs and the number of packets received from them.
        """
        with self._rx_stats_lock:
            ret = copy.deepcopy(self._rx_stats)
        return ret

    @property
    def transmission_count(self) -> int:
        """The number of packets transmitted over LoRa.

        Returns:
            The number of packets transmitted by the connected node.
        """
        with self._tx_stats_lock:
            ret = self._tx_stats
        return ret
