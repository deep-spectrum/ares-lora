from ares_lora_serial_core import _AresSerial, AresTimeout, _AresLoraConfig, AresThreadTerminate
from typing import Callable
from enum import IntEnum
from dataclasses import dataclass, asdict
import functools
from .errno import strerror
import logging
from .utils import check_serial_port
from threading import Lock, Thread, Event
import copy
import ctypes
import threading
from weakref import WeakSet
from queue import Queue, Empty
from datetime import datetime, timedelta
from pathlib import Path

logger = logging.getLogger("ares_lora")


class LoraException(Exception):
    """Exception class for LoRa related exceptions."""

    def __init__(self, code: int, extra_str: str | None = None):
        err = strerror(code)
        if extra_str is not None:
            err = f"{extra_str}: {err}"
        super().__init__(err)


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


# @dataclass
# class LoraSerialConfig:
#     """Configurations for the LoRa serial driver.
#
#     Attributes:
#         port: The serial port to connect to.
#         response_timeout: The amount of time (in seconds) to wait for a response from the firmware.
#         rx_period: How often (in seconds) the serial driver polls the serial receive buffer.
#         serial_timeout: The serial RX timeout (in seconds).
#     """
#     port: str = ""
#     response_timeout: float = 2.0
#     rx_period: float = 0.1
#     serial_timeout: float = 0.1


@dataclass
class LogMessage:
    msg_id: int
    last_part: int
    total_parts: int
    msg: str
    transmitted: bool = False


class BleState(IntEnum):
    """Different states the BLE can be in (except for request).

    Attributes:
        OFF: BLE is turned off.
        ON: BLE is solid on.
        REQUEST: Request the current BLE state.
    """
    OFF = 0
    ON = 1
    REQUEST = 2


def lora_serial_command(func):
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except AresTimeout as e:
            raise TimeoutError(str(e))

    return wrapper


_instances = WeakSet()


def _shutdown_drivers():
    global _instances
    for x in _instances:
        # noinspection protected-member
        x._stop_device_driver_noexcept()


# Need this since the threading module wants to run its shutdown sequence before object deletion...
# noinspection unresolved-references,protected-member
threading._register_atexit(_shutdown_drivers)


class LoraSerial:
    """LoRa serial driver python implementation. Works only on Linux."""

    def __init__(self, port: str | Path, **kwargs: float):
        """Initializes the LoRa driver.

        Args:
            port: The port the lora node is connected to
            kwargs: Keyword arguments

        Keyword Args:
            serial_timeout (float): The serial reception timeout
            rx_period (float): The period to poll the receive buffer

        Raises:
            ValueError: Empty port configuration.
            IOError: Port not found.
        """
        if isinstance(port, Path):
            port = str(port)

        if not port:
            raise ValueError("Invalid port")
        if not check_serial_port(port):
            raise IOError(f"Cannot open port {port}: Does not exist")

        self._start_hook_lock = threading.Lock()
        self._start_hook: Callable[[int, int], None] | None = None

        self._log_hook_lock = threading.Lock()
        self._log_hook: Callable[[int, str], None] | None = None

        self._abortion_hook_lock = threading.Lock()
        self._abortion_hook: Callable[[int, bool], None] | None = None

        self._run_ready_hook_lock = threading.Lock()
        self._run_ready_hook: Callable[[int, bool], None] | None = None

        self._dev = _AresSerial(port, **kwargs)
        self._nodes: dict[int, int] = {}
        self._log_msg: dict[int, LogMessage] = {}

        self._rx_stats: dict[int, int] = {}
        self._rx_stats_lock = Lock()

        self._tx_stats: int = 0
        self._tx_stats_lock = Lock()

        self._logger = logger

        self._start_thread: Thread | None = None
        self._log_thread: Thread | None = None
        self._pkt_rx_thread: Thread | None = None
        self._pkt_tx_thread: Thread | None = None
        self._abort_event_thread: Thread | None = None
        self._node_ready_event_thread: Thread | None = None

        self._wait_ble_connect_event: Thread | None = None
        self._wait_ble_subscribe_event: Thread | None = None

        self._ble_connect_events: Queue[bool] = Queue()
        self._ble_subscribe_events: Queue[tuple[bool, ...]] = Queue()

        self._driver_started = Event()

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
                with self._start_hook_lock:
                    if self._start_hook is not None:
                        self._start_hook(sec, usec)

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

                with self._log_hook_lock:
                    if self._log_hook is not None:
                        self._log_hook(src_id, self._log_msg[src_id].msg)

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
                count = self._dev.wait_packet_tx_event()
            except AresThreadTerminate:
                break

            with self._tx_stats_lock:
                self._tx_stats += count

    def _abort_event_handler(self):
        while True:
            try:
                broadcast, source_id = self._dev.wait_abortion_event()
            except AresThreadTerminate:
                break

            with self._abortion_hook_lock:
                if self._abortion_hook is not None:
                    self._abortion_hook(source_id, broadcast)

    def _node_ready_event_handler(self):
        while True:
            try:
                source_id, broadcast = self._dev.wait_run_ready_event()
            except AresThreadTerminate:
                break

            with self._run_ready_hook_lock:
                if self._run_ready_hook is not None:
                    self._run_ready_hook(source_id, broadcast)

    def _ble_connect_event_handle(self):
        while True:
            try:
                connected = self._dev.wait_ble_connection_event()
            except AresThreadTerminate:
                break

            self._ble_connect_events.put(connected)

    def _ble_subscribe_event_handle(self):
        while True:
            try:
                subscriptions: tuple[bool, ...] = self._dev.wait_ble_subscribe_event()
            except AresThreadTerminate:
                break

            self._ble_subscribe_events.put(subscriptions)

    @staticmethod
    def _check_ret_code(code: int | tuple[int, ...] | dict[str, int]):
        if isinstance(code, int):
            if code != 0:
                raise LoraException(code)
            return
        if isinstance(code, tuple):
            for c in code:
                if c != 0:
                    raise LoraException(c)
            return
        for key, c in code.items():
            if c != 0:
                raise LoraException(c, key)

    @lora_serial_command
    def setting(self, setting_id: SettingId, **kwargs: int | float) -> int | None:
        """Set or retrieve a LoRa firmware setting.

        Args:
            setting_id: The setting to read or write to.
            kwargs: Keyword arguments.

        Keyword Args:
            value (int): The new setting value.
            response_timeout (float): The maximum time to wait for a response.

        Returns:
            If writing a setting, None. If reading a setting, the value of the setting.

        Raises:
            TimeoutError: No response from the firmware within the configured timeout.
            LoraException: Firmware responded with an error code.
        """
        ret, value = self._dev.setting(id=setting_id.value, **kwargs)
        self._check_ret_code(ret)
        if value is not None:
            return value[0]
        return None

    @lora_serial_command
    def lora_config(self, config: LoraConfig, **kwargs: float):
        """Configure the LoRa modem.

        Args:
            config: The LoRa modem configurations.
            kwargs: Keyword arguments

        Keyword Args:
            response_timeout (float): The maximum time to wait for a response.

        Raises:
            TimeoutError: No response from the firmware within the configured timeout.
            LoraException: Firmware responded with an error code.
        """
        args = asdict(config)
        for key in args.keys():
            if not isinstance(args[key], int):
                args[key] = args[key].value
        configs_ = _AresLoraConfig(**args)
        ret = self._dev.lora_config(configs_, **kwargs)
        self._check_ret_code(ret[0])

    @lora_serial_command
    def led(self, led_id: int, **kwargs: LoraLedState | float | int) -> LoraLedState | None:
        """Set or retrieve the state of the LED.

        Args:
            led_id: The ID/number of the LED to fetch/set the state of.
            kwargs: Keyword arguments

        Keyword Args:
            state (LoraLedState | int): The new LED state
            response_timeout (float): The maximum time to wait for a response.

        Returns:
            The current LED state if state is LoraLedState.FETCH. None otherwise.

        Raises:
            TimeoutError: No response from the firmware within the configured timeout.
            LoraException: Firmware responded with an error code.
        """
        if led_id > ctypes.c_uint8(-1).value:
            raise ValueError(f"led_id is {led_id}. Valid range: [0, {ctypes.c_uint8(-1).value}]")
        ret = self._dev.led(id=led_id, **kwargs)
        self._check_ret_code(ret[0])
        if ret[1] is not None:
            return LoraLedState(ret[1][0])
        return None

    @lora_serial_command
    def version(self, **kwargs: float) -> tuple[tuple[int, int, int], tuple[int, int, int], tuple[int, int, int]]:
        """Retrieves all the firmware version information.

        Args:
            kwargs: Keyword arguments

        Keyword Args:
            response_timeout (float): The maximum time to wait for a response.

        Returns:
            A tuple of versions. The first tuple is the application version, the second tuple is the ncs version, and the third tuple is the kernel version.

        Raises:
            TimeoutError: No response from the firmware within the configured timeout.
            LoraException: Firmware responded with an error code.

        Notes:
            A version tuple is as follows: (major, minor, patch).
        """
        ret = self._dev.version(**kwargs)
        self._check_ret_code(ret[0])
        return ret[1]

    @lora_serial_command
    def reboot(self, **kwargs: int | float):
        """Reboot the connected device.

        Args:
            kwargs: Keyword arguments.

        Keyword Args:
            delay (int): The amount of seconds to delay the reboot by
            response_timeout (float): The maximum time to wait for a response.

        Raises:
            TimeoutError: No response from the firmware within the configured timeout.
            LoraException: Firmware responded with an error code.

        Notes:
            If the reboot was a success, then the driver needs to be started again. This will
            automatically stop the driver. If the port is unable to be reopened, then a new
            driver instance will be needed.
        """
        ret = self._dev.reboot(**kwargs)
        self.stop_driver()
        self._check_ret_code(ret[0])

    @lora_serial_command
    def start(self, sec: int, usec: int, **kwargs: int | float | bool) -> bool | None:
        """Send start time over LoRa

        Args:
            sec: The seconds part of the time to start.
            usec: The microseconds part of the time to start.
            kwargs: Keyword arguments

        Keyword Args:
             destination (int): The destination ID.
             response_timeout (float): The maximum time to wait for a response.
             ack_timeout (float): Maximum time to wait for a LoRa acknowledgement.
             broadcast (bool): Flag indicating if the message should be broadcasted.
             retries (int): The amount of times to retry if no ACK was received.

        Returns:
            `None` if a broadcast message. `True` if the message was acknowledged, `False` otherwise.

        Raises:
            ValueError: The start time is invalid.
            TimeoutError: No response from the firmware within the timeout.
            LoraException: Firmware responded with an error code.
        """
        if sec < 0 or usec < 0:
            raise ValueError("Time must be positive")
        ret = self._dev.start(sec, usec, **kwargs)
        self._check_ret_code(ret[0])
        if ret[1] is not None:
            return ret[1][0]
        return None

    @lora_serial_command
    def poll(self, **kwargs: int | float) -> bool:
        """Poll a node on the LoRa network for a heartbeat.

        Args:
            kwargs: Keyword arguments.

        Keyword Args:
             destination (int): The destination ID.
             response_timeout (float): The maximum time to wait for a response.
             ack_timeout (float): Maximum time to wait for a LoRa acknowledgement.
             retries (int): The amount of times to retry if no ACK was received.

        Raises:
            ValueError: The node ID is invalid.
            TimeoutError: No response from firmware within the timeout.
            TimeoutError: No response from the polled node within the poll response timeout.
            LoraException: Firmware responded with an error code.

        Returns:
            The ready status of the polled node.
        """
        ret = self._dev.poll(**kwargs)
        self._check_ret_code(ret[0])
        return ret[1][0]

    @lora_serial_command
    def log(self, **kwargs: str | int | float | bool) -> bool | None:
        """Send a log message over LoRa.

        Args:
            kwargs: Keyword arguments

        Keyword Args:
            message (str): The message to send.
            destination (int): The destination ID.
            response_timeout (float): The maximum time to wait for a response.
            ack_timeout (float): Maximum time to wait for a LoRa acknowledgement.
            broadcast (bool): Flag indicating if the message should be broadcasted.
            retries (int): The amount of times to retry if no ACK was received.

        Returns:
            `None` if a broadcast message. `True` if all chunks of the log message were ACK'ed, `False` otherwise.

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
        ret = self._dev.log(**kwargs)
        self._check_ret_code(ret[0])
        if ret[1] is not None:
            return all(ret[1])
        return None

    @lora_serial_command
    def abort(self, **kwargs: int | float | bool) -> bool | None:
        """Send an abortion message over LoRa.

        Args:
            kwargs: Keyword arguments

        Keyword Args:
            destination (int): The destination ID.
            response_timeout (float): The maximum time to wait for a response.
            ack_timeout (float): Maximum time to wait for a LoRa acknowledgement.
            broadcast (bool): Flag indicating if the message should be broadcasted.
            retries (int): The amount of times to retry if no ACK was received.

        Returns:
            `None` if a broadcast message. `True` if the message was ACK'ed, `False` otherwise.

        Raises:
            TimeoutError: No response from the firmware within the configured timeout.
            ValueError: The node ID is invalid.
            LoraException: Firmware responded with an error code.
        """
        ret = self._dev.abort(**kwargs)
        self._check_ret_code(ret[0])
        if ret[1] is not None:
            return ret[1][0]
        return None

    @lora_serial_command
    def send_node_configs(self, **kwargs: float | int | datetime) -> dict[str, bool]:
        """Send node configurations over LoRa.

        Args:
            kwargs: Keyword arguments.

        Keyword Args:
            destination (int): The destination ID.
            response_timeout (float): The maximum time to wait for a response.
            ack_timeout (float): Maximum time to wait for a LoRa acknowledgement.
            retries (int): The amount of times to retry if no ACK was received.
            folder_dt (datetime): The save folder timestamp for naming purposes.
            bandwidth (float): The bandwidth for the collection run.
            center_freq (float): The center frequency for the collection run.
            duration (int): The duration (in seconds) of the run.
            ref_level (float): The reference level of the run.

        Returns:
            A dictionary of configs sent over LoRa and a flag indicating if the config was received by the other node.

        Raises:
            TimeoutError: No response from the firmware within the configured timeout.
            TimeoutError: No acknowledgement from the destination node.
            LoraException: Firmware responded with an error code.
        """
        results: dict[str, tuple[tuple[int], tuple[bool]]] = self._dev.node_config(**kwargs)
        ret:dict[str, bool] = {}
        codes:dict[str, int] = {}
        for config, result in results.items():
            ret[config] = result[1][0]
            codes[config] = result[0][0]
        self._check_ret_code(codes)
        return ret

    @lora_serial_command
    def poll_node_config(self, *args: str, **kwargs: float | int) -> dict[str, float | int | datetime | None]:
        """Poll a node for its configurations.

        Args:
            *args: The configurations to poll for.
                Valid arguments are "folder_dt", "bandwidth", "center_freq", "duration", and "ref_level".
            **kwargs: Keyword arguments

        Keyword Args:
            destination (int): The destination ID.
            response_timeout (float): The maximum time to wait for a response.
            ack_timeout (float): Maximum time to wait for a LoRa acknowledgement.
            retries (int): The amount of times to retry if no ACK was received.

        Returns:
            dict[str, float | int | datetime]: If a value is `None`, then polling for that configuration failed.

            The keys are the same values as args. Any invalid args will not be present.

        Raises:
            TimeoutError: No response from the firmware within the configured timeout.
            TimeoutError: No acknowledgement from the destination node.
            LoraException: Firmware responded with an error code.
        """
        ret_: dict[str, tuple[tuple[int], tuple[float | int | datetime | None]]] = self._dev.node_config_poll(*args, **kwargs)
        codes: dict[str, int] = {}
        ret: dict[str, float | int | datetime | None] = {}
        for config, value in ret_.items():
            codes[config] = value[0][0]
            ret[config] = value[1][0]
        self._check_ret_code(codes)
        return ret

    @lora_serial_command
    def notify_run_ready(self, **kwargs: int | float | bool) -> bool | None:
        """Send a notification over LoRa to tell that the nodes should get ready to collect data.

        Args:
            kwargs: Keyword arguments

        Keyword Args:
            destination (int): The destination ID.
            response_timeout (float): The maximum time to wait for a response.
            ack_timeout (float): Maximum time to wait for a LoRa acknowledgement.
            broadcast (bool): Flag indicating if the message should be broadcasted.
            retries (int): The amount of times to retry if no ACK was received.

        Returns:
            `None` if a broadcast message. `True` if the message was acknowledged, `False` otherwise.

        Raises:
            TimeoutError: No response from the firmware within the configured timeout.
            TimeoutError: No acknowledgement from the destination node.
            ValueError: The node ID is invalid.
            LoraException: Firmware responded with an error code.
        """
        ret = self._dev.notify_run_ready(**kwargs)
        self._check_ret_code(ret[0])
        if ret[1] is not None:
            return ret[1][0]
        return None

    @lora_serial_command
    def ble_state(self, state: BleState = BleState.REQUEST) -> BleState | None:
        """Retrieve or update the BLE state.

        Args:
            state: The new BLE state. If BleState.REQUEST, returns the current BLE state.

        Returns:
            The current BLE state if state is BleState.REQUEST. None otherwise.

        Raises:
            TimeoutError: No response from the firmware within the configured timeout.
            LoraException: Firmware responded with an error code.
        """
        ret = self._dev.ble_state(state=state.value)
        self._check_ret_code(ret[0])
        if state == BleState.REQUEST:
            return BleState(ret[1][0])
        return None

    @lora_serial_command
    def ble_disconnect(self):
        """Terminate the current BLE connection, but keep BLE active.

        Raises:
            TimeoutError: No response from the firmware within the configured timeout.
            LoraException: Firmware responded with an error code.
        """
        ret = self._dev.ble_disconnect()
        self._check_ret_code(ret[0])

    # @lora_serial_command
    # def ble_send(self, data: bytes):
    #     """Send data over the BLE connection.
    #
    #     Args:
    #         data: Data to send over BLE.
    #
    #     Raises:
    #         TimeoutError: No response from the firmware within the configured timeout.
    #         LoraException: Firmware responded with an error code.
    #     """
    #     codes = self._dev.ble_send_image(data)
    #     self._check_ret_code(codes)

    def wait_connection_changed_event(self, block: bool = True, timeout: float | None = None) -> bool:
        """Wait for a connection event from BLE.

        Args:
            block: Block execution if there are no items in the queue.
            timeout: The maximum amount of seconds to wait for a connection event.

        Notes:
            See queue.Queue.get for behavior on different parameter combinations.

        Returns:
            True if BLE has connected, False if BLE has disconnected.
        """
        return self._ble_connect_events.get(block, timeout)

    def wait_subscription_change_event(self, block: bool = True, timeout: float | None = None) -> tuple[bool, ...]:
        """Wait for subscription update event from BLE.

        Args:
            block: Block execution if there are no items in the queue.
            timeout: The maximum amount of seconds to wait for a connection event.

        Notes:
            See queue.Queue.get for behavior on different parameter combinations.

        Returns:

        """
        return self._ble_subscribe_events.get(block, timeout)

    def clear_ble_events(self):
        """Clear the event BLE event queues."""
        try:
            while True:
                self._ble_connect_events.get_nowait()
        except Empty:
            pass

        try:
            while True:
                self._ble_subscribe_events.get_nowait()
        except Empty:
            pass

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

    def register_start_hook(self, hook: Callable[[int, int], None] | None):
        """Register a start event hook.

        Args:
            hook: The function to call when a start event occurs. `None` to unregister the hook.

        Notes:
            The hook signature is [start_time_sec, start_time_usec] -> None.

        Warning:
            Registering or unregistering a hook performs a blocking action. It is highly recommended
            that hooks should be kept short and fast and offload work to other threads.
        """
        with self._start_hook_lock:
            self._start_hook = hook

    def register_log_hook(self, hook: Callable[[int, str], None] | None):
        """Register a log event hook.

        Args:
            hook: The function to call when a log event occurs. `None` to unregister the hook.

        Notes:
            The hook signature is [source_id, log_message] -> None.

        Warning:
            Registering or unregistering a hook performs a blocking action. It is highly recommended
            that hooks should be kept short and fast and offload work to other threads.
        """
        with self._log_hook_lock:
            self._log_hook = hook

    def register_abortion_hook(self, hook: Callable[[int, bool], None] | None):
        """Register an abortion event hook.

        Args:
            hook: The function to call when an abortion event occurs. `None` to unregister the hook.

        Notes:
            The hook signature is [source_id, broadcast] -> None.

        Warning:
            Registering or unregistering a hook performs a blocking action. It is highly recommended
            that hooks should be kept short and fast and offload work to other threads.
        """
        with self._abortion_hook_lock:
            self._abortion_hook = hook

    def register_run_ready_hook(self, hook: Callable[[int, bool], None] | None):
        """Register a run ready hook.

        Args:
            hook: The function to call when a run ready event occurs. `None` to unregister the hook.

        Notes:
            The hook signature is [source_id, broadcast] -> None.

        Warning:
            Registering or unregistering a hook performs a blocking action. It is highly recommended
            that hooks should be kept short and fast and offload work to other threads.
        """
        with self._run_ready_hook_lock:
            self._run_ready_hook = hook

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

    def _start_driver(self):
        self._dev.start_driver()

        self._start_thread = Thread(target=self._start_event_handle)
        assert isinstance(self._start_thread, Thread)
        self._start_thread.start()

        self._log_thread = Thread(target=self._log_event_handle)
        assert isinstance(self._log_thread, Thread)
        self._log_thread.start()

        self._pkt_rx_thread = Thread(target=self._pkt_rx_event_handle)
        assert isinstance(self._pkt_rx_thread, Thread)
        self._pkt_rx_thread.start()

        self._pkt_tx_thread = Thread(target=self._pkt_tx_done_event_handle)
        assert isinstance(self._pkt_tx_thread, Thread)
        self._pkt_tx_thread.start()

        self._wait_ble_connect_event = Thread(target=self._ble_connect_event_handle)
        assert isinstance(self._wait_ble_connect_event, Thread)
        self._wait_ble_connect_event.start()

        self._wait_ble_subscribe_event = Thread(target=self._ble_subscribe_event_handle)
        assert isinstance(self._wait_ble_subscribe_event, Thread)
        self._wait_ble_subscribe_event.start()

        self._abort_event_thread = Thread(target=self._abort_event_handler)
        assert isinstance(self._abort_event_thread, Thread)
        self._abort_event_thread.start()

        self._node_ready_event_thread = Thread(target=self._node_ready_event_handler)
        assert isinstance(self._node_ready_event_thread, Thread)
        self._node_ready_event_thread.start()

        self._driver_started.set()

        global _instances
        _instances.add(self)

    def _stop_device_driver_noexcept(self):
        if not self._driver_started.is_set():
            return
        self._dev.cancel_events()

    def start_driver(self):
        """Starts execution of the LoRa driver."""

        if self._driver_started.is_set():
            raise RuntimeError("Driver already started")

        self._start_driver()

    def _stop_driver(self):
        self._dev.stop_driver()

        if self._start_thread is not None:
            self._start_thread.join()
            self._start_thread = None

        if self._log_thread is not None:
            self._log_thread.join()
            self._log_thread = None

        if self._pkt_rx_thread is not None:
            self._pkt_rx_thread.join()
            self._pkt_rx_thread = None

        if self._pkt_tx_thread is not None:
            self._pkt_tx_thread.join()
            self._pkt_tx_thread = None

        if self._wait_ble_subscribe_event is not None:
            self._wait_ble_subscribe_event.join()
            self._wait_ble_subscribe_event = None

        if self._wait_ble_connect_event is not None:
            self._wait_ble_connect_event.join()
            self._wait_ble_connect_event = None

        if self._abort_event_thread is not None:
            self._abort_event_thread.join()
            self._abort_event_thread = None

        if self._node_ready_event_thread is not None:
            self._node_ready_event_thread.join()
            self._node_ready_event_thread = None

        self._driver_started.clear()

    def stop_driver(self):
        """Stops execution of the LoRa driver."""

        if not self._driver_started.is_set():
            raise RuntimeError("Driver not started")

        self._stop_driver()

        global _instances
        if self in _instances:
            _instances.remove(self)

    def __enter__(self):
        self.start_driver()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop_driver()

    def __del__(self):
        self._stop_driver()

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

    @property
    def ready(self) -> bool:
        """The ready status of this node.

        Returns:
            The currently set ready status.
        """
        return self._dev.ready

    @ready.setter
    def ready(self, value: bool):
        """The ready status of this node.

        Args:
            value: The new ready status.
        """
        self._dev.ready = value

    @property
    def node_configs(self) -> dict[str, float | int | datetime]:
        return self._dev.node_configs


# class BleTransfer:
#     """Context manager for transferring data over BLE. Only works on Linux."""
#
#     def __init__(self, serial: LoraSerial, timeout: float = 60.0, exit_timeout: float = 1.0):
#         """Initializes the BleTransfer instance.
#
#         Args:
#             serial: The LoRaSerial instance to transfer data over.
#             timeout: The amount of time allowed to wait for a connection and for all the required attributes to be subscribed to.
#             exit_timeout: The amount of time to wait for the central device to disconnect before turning off BLE forcefully.
#
#         Raises:
#             ValueError: One or more of the timeout values are invalid.
#         """
#         self._dev = serial
#
#         if timeout < 0:
#             raise ValueError("Timeout must be positive")
#         if exit_timeout < 0:
#             raise ValueError("Timeout must be positive")
#
#         self._timeout = timeout
#         self._exit_timeout = exit_timeout
#
#     def _wait_connect_event(self):
#         try:
#             self._dev.wait_connection_changed_event(True, self._timeout)
#         except Empty:
#             raise TimeoutError("Could not establish BLE connection")
#
#     def _wait_subscriptions(self):
#         try:
#             subs = (False,)
#             timeout_time = datetime.now() + timedelta(seconds=self._timeout)
#             while not all(subs) and datetime.now() < timeout_time:
#                 subs = self._dev.wait_subscription_change_event(True, self._timeout)
#             if not all(subs):
#                 raise TimeoutError("Central device did not subscribe to all required attributes")
#         except Empty:
#             raise TimeoutError("Central device did not subscribe to all required attributes")
#
#     def __enter__(self):
#         if self._dev.ble_state() == BleState.ON:
#             raise RuntimeError("BLE must be disabled before using this context manager")
#         self._dev.clear_ble_events()
#         self._dev.ble_state(BleState.ON)
#         try:
#             self._wait_connect_event()
#             self._wait_subscriptions()
#         except:
#             self._dev.ble_state(BleState.OFF)
#             raise
#         return self
#
#     def __exit__(self, exc_type, exc_val, exc_tb):
#         try:
#             # Wait for central device to disconnect
#             self._dev.wait_connection_changed_event(True, self._exit_timeout)
#         except Empty:
#             pass
#         self._dev.ble_state(BleState.OFF)
#
#     def write(self, data: bytes):
#         """Write data over the BLE connection.
#
#         Args:
#             data: The data to send over BLE. Automatically chunked.
#         """
#         self._dev.ble_send(data)
