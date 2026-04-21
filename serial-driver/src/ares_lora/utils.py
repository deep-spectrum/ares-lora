import serial.tools.list_ports as list_ports

TARGETS = (
    'CMU LoRa',
)


def _find_ports(targets: tuple[str, ...]) -> dict[str, list[str]]:
    ports = list(list_ports.comports())
    ret: dict[str, list[str]] = {}

    for port in ports:
        name = f"{port.manufacturer} {port.product}"
        if name in targets:
            if name in ret:
                ret[name].append(port.device)
            else:
                ret[name] = [port.device]
    return ret


def check_serial_port(port: str) -> bool:
    ports = _find_ports(TARGETS)
    for ports_ in ports.values():
        if port in ports_:
            return True
    return False


def find_ports(targets: tuple[str, ...] | None = None) -> dict[str, list[str]]:
    """Finds valid ports that match the description given by the valid targets tuple.

    Args:
        targets: The valid targets for ports. This is a tuple of strings described as such: f"{manufacturer} {product}".

    Returns: A dictionary where the key is a valid target and the value is a list of ports associated with that target.
    """
    if targets is None:
        targets: tuple[str, ...] = TARGETS
    return _find_ports(targets)
