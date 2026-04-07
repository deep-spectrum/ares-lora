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


class AresDfu:
    def __init__(self, port: str):
        self._port = port

    async def _read_image_states(self, timeout: float) -> list[ImageState]:
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
