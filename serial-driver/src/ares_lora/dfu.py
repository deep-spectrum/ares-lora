import asyncio
from smp import error as smperror
from smp.os_management import OS_MGMT_RET_RC
from smpclient import SMPClient
from smpclient.generics import error, error_v1, error_v2, success
from smpclient.requests.image_management import ImageStatesRead, ImageStatesWrite
from smp.image_management import ImageState
from smpclient.requests.os_management import ResetWrite
from smpclient.transport.serial import SMPSerialTransport
from pathlib import Path
from dataclasses import dataclass
from smpclient import logger
import logging
from abc import ABC, abstractmethod
import time
from typing import Type, Callable, Literal

logger.setLevel(logging.CRITICAL + 10)


class ImageManagerException(Exception):
    """Base class for DFU exceptions"""


@dataclass
class AresImageStates:
    """Data class for image states.

    Attributes:
        slot: The image slot.
        version: The version of the image.
        hash: The hash of the image.
        bootable: Flag indicating if the image is bootable.
        pending: Flag indicating image is pending.
        confirmed: Flag indicating image has been confirmed.
        active: Flag indicating image is active.
        permanent: Flag indicating the image is permanent.
    """
    slot: int = 0
    version: str = ''
    hash: bytes = b''
    bootable: bool = False
    pending: bool = False
    confirmed: bool = False
    active: bool = False
    permanent: bool = False

    @classmethod
    def from_smp_lib(cls, states: ImageState) -> 'AresImageStates':
        """Construct an `AresImageStates` object from an `ImageState` object.

        Args:
            states: The `ImageStates` object.

        Returns:
            A constructed `AresImageStates` object.
        """
        return cls(slot=states.slot, version=states.version, hash=states.hash, bootable=states.bootable,
                   pending=states.pending, confirmed=states.confirmed, active=states.active, permanent=states.permanent)


class AresUploadStatusBase(ABC):
    """Base class for upload status CLI utilities. This must be subclassed."""

    def __init__(self, image_len: int):
        """Initializes the base class.

        Args:
            image_len: The length of the image in bytes.
        """
        self._len = image_len

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        pass

    @abstractmethod
    def update(self, offset: int) -> None:
        """Update method of the base class. This method must be overridden.

        Args:
            offset: The current offset for image writing.
        """
        pass


class AresUploadStatus(AresUploadStatusBase):
    def __init__(self, image_len: int):
        super().__init__(image_len)
        self._start_s: float | None = None

    def __enter__(self):
        self._start_s = time.time()
        return super().__enter__()

    def __exit__(self, exc_type, exc_val, exc_tb):
        print("")

    def update(self, offset: int) -> None:
        print(
            f"\rUploaded {offset:,} / {self._len:,} Bytes | {offset / (time.time() - self._start_s) / 1000:.2f} KB/s    ",
            end="", flush=True)


class AresDfu:
    """Device firmware update manager for Ares LoRa."""

    def __init__(self, port: str, verbose: bool = False,
                 upload_status_cls: Type[AresUploadStatusBase] = AresUploadStatus):
        """Initializes AresDfu.

        Args:
            port: The port that supports the Dfu protocol.
            verbose: Run DFU in verbose mode.
            upload_status_cls: The upload status class.
        """
        self._port = port
        self._verbose = verbose
        self._upload_status_cls = upload_status_cls

    async def _read_image_states(self, timeout: float = 2.0) -> list[ImageState]:
        async with SMPClient(SMPSerialTransport(), self._port) as client:
            response = await client.request(ImageStatesRead(), timeout_s=timeout)
            if success(response):
                return response.images
            elif error(response):
                raise ImageManagerException(f"Received error: {response}")
            else:
                raise ImageManagerException(f"Unknown response: {response}")

    def read_image_states(self, timeout: float = 2.0) -> tuple[AresImageStates, ...]:
        """Read the images and their states from the microcontroller.

        Args:
            timeout: Timeout in seconds to complete this operation.

        Returns:
            A tuple of AresImageStates objects.

        Raises:
            ImageManagerException: SMP Response errors or unknown responses.
        """
        states = asyncio.run(self._read_image_states(timeout))
        ret = [AresImageStates.from_smp_lib(state) for state in states]
        return tuple(ret)

    async def __upload_image_run(self, image: bytes, slot: int, timeout_s: float = 2.5,
                                 update_cb: Callable[[int], None] | None = None) -> None:
        async with SMPClient(SMPSerialTransport(), self._port) as client:
            async for offset in client.upload(image, slot, first_timeout_s=timeout_s, use_sha=True):
                if update_cb is not None:
                    update_cb(offset)
            await self._mark_image_pending(1, timeout_s)

    async def __upload_image(self, image: Path | str, slot: int, update_status: Type[AresUploadStatusBase] | None,
                             timeout_s: float = 2.5):
        image_path = Path(image)
        with open(image_path, "rb") as f:
            image: bytes = f.read()

        if update_status is None:
            await self.__upload_image_run(image, slot, timeout_s, None)
            return
        with update_status(len(image)) as status:
            await self.__upload_image_run(image, slot, timeout_s, status.update)

    async def _upload_image(self, image_path: str | Path, slot: int = 0, retries: int = 3, timeout_s: float = 2.5):
        status_cls = None
        if self._verbose:
            status_cls = self._upload_status_cls
        while True:
            try:
                await self.__upload_image(image_path, slot, status_cls, timeout_s)
            except Exception as e:
                retries -= 1
                if retries < 0:
                    raise ImageManagerException(e)
                await asyncio.sleep(1)
            else:
                break

    def upload_image(self, image_path: str | Path, slot: int = 0, retries: int = 3, timeout: float = 40.0) -> None:
        """Upload an image to the microcontroller.

        Args:
            image_path: The file path to the image to upload.
            slot: The slot to upload the image to.
            retries: The maximum amount of retries.
            timeout: Timeout of the first write request in seconds.

        Raises:
            ImageManagerException: Upload image failed.
        """
        asyncio.run(self._upload_image(image_path, slot, retries, timeout))

    async def _reset_mcu(self, force: Literal[0, 1] = 0, timeout: float = 2.5):
        async with SMPClient(SMPSerialTransport(), self._port) as client:
            response = await client.request(ResetWrite(force=force), timeout_s=timeout)
            if error_v1(response):
                if response.rc != smperror.MGMT_ERR.EOK:
                    raise ImageManagerException("Response is not OK")
            elif error_v2(response):
                if response.err.rc != OS_MGMT_RET_RC.OK:
                    raise ImageManagerException("Response is not OK")

    def reset_mcu(self, force: bool = False, timeout: float = 2.5):
        """Reset the microcontroller.

        Args:
            force: Force the reset. This will cause a cold restart as opposed to a warm restart.
            timeout: The timeout of the reset request in seconds.

        Raises:
            ImageManagerException: Reset request failed.
        """
        if force:
            force: Literal[0, 1] = 1
        else:
            force: Literal[0, 1] = 0
        asyncio.run(self._reset_mcu(force, timeout))

    async def _confirm_image(self, slot: int, force: bool, timeout: float) -> None:
        images = await self._read_image_states(timeout)
        if slot < 0 or slot >= len(images):
            raise ImageManagerException("Invalid slot")

        image = images[slot]
        if not image.active or not image.bootable or image.confirmed:
            # Invalid image
            if not image.active and not force:
                raise ImageManagerException("Cannot confirm image that is not active")
            if not image.bootable:
                raise ImageManagerException("Cannot confirm image that is not bootable")
            if image.confirmed and not force:
                raise ImageManagerException("Cannot confirm image that has been confirmed already")
        async with SMPClient(SMPSerialTransport(), self._port) as client:
            await client.request(ImageStatesWrite(hash=image.hash, confirm=True), timeout_s=timeout)

    def confirm_image(self, slot: int = 0, force: bool = False, timeout: float = 2.5):
        """Confirm an image.

        Args:
            slot: The image to confirm (index).
            force: Force the confirmation.
            timeout: The response timeout in seconds.

        Raises:
            ImageManagerException: Unable to confirm image.
        """
        asyncio.run(self._confirm_image(slot, force, timeout))

    async def _mark_image_pending(self, slot: int = 1, timeout: float = 2.5):
        images = await self._read_image_states(timeout)
        if slot >= len(images):
            raise ImageManagerException("Cannot mark image as pending since it doesn't exist")
        async with SMPClient(SMPSerialTransport(), self._port) as client:
            await client.request(ImageStatesWrite(hash=images[slot].hash), timeout_s=timeout)

    def mark_image_pending(self, slot: int = 1, timeout: float = 2.5):
        """Mark an image as pending.

        Args:
            slot: The image to mark as pending (index).
            timeout: The timeout for the request in seconds.

        Raises:
            ImageManagerException: Unable to mark image as pending.
        """
        asyncio.run(self._mark_image_pending(slot, timeout))
