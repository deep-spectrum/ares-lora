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
    pass


@dataclass
class AresImageStates:
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
        return cls(slot=states.slot, version=states.version, hash=states.hash, bootable=states.bootable,
                   pending=states.pending, confirmed=states.confirmed, active=states.active, permanent=states.permanent)


class AresUploadStatusBase(ABC):
    def __init__(self, image_len: int):
        self._len = image_len

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        pass

    @abstractmethod
    def update(self, offset: int) -> None:
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
    def __init__(self, port: str, verbose: bool = False,
                 upload_status_cls: Type[AresUploadStatusBase] = AresUploadStatus):
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
        states = asyncio.run(self._read_image_states(timeout))
        ret = [AresImageStates.from_smp_lib(state) for state in states]
        return tuple(ret)

    async def _mark_test_image(self, client: SMPClient):
        images = await self._read_image_states()
        await client.request(ImageStatesWrite(hash=images[1].hash))

    async def __upload_image_run(self, image: bytes, slot: int, timeout_s: float = 2.5,
                                 update_cb: Callable[[int], None] | None = None) -> None:
        async with SMPClient(SMPSerialTransport(), self._port) as client:
            async for offset in client.upload(image, slot, first_timeout_s=timeout_s, use_sha=True):
                if update_cb is not None:
                    update_cb(offset)
            await self._mark_test_image(client)

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

    def upload_image(self, image_path: str | Path, slot: int = 0, retries: int = 3, timeout: float = 2.5) -> None:
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
        if force:
            force: Literal[0, 1] = 1
        else:
            force: Literal[0, 1] = 0
        asyncio.run(self._reset_mcu(force, timeout))
