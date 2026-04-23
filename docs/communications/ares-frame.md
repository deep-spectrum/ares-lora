# Ares Frames

The firmware communicates with the host computer over serial with a frames. These frames are called "Ares Frames." The
frames have a common structure and consist of a variety of frames types. The frame structure and frame types are 
described as follows:

| Field        | Header | Payload Length | Type | Payload | Footer |
|:-------------|:------:|:--------------:|:----:|:-------:|:------:|
| Size (bytes) |   1    |       2        |  1   |    n    |   1    |
| Value        |   ^    |       n        | ...  |   ...   |   @    |


| Type          | Value | Direction | Description                                                                      |
|:--------------|:-----:|:---------:|:---------------------------------------------------------------------------------|
| SETTING       |   0   |   TX/RX   | Configure or read persistent firmware settings                                   |
| START         |   1   |   TX/RX   | Time to start I/Q data collection                                                |
| LORA_CONFIG   |   2   |    RX     | Set new LoRa modem configurations                                                |
| LED           |   3   |   TX/RX   | Set or read the LED state/action                                                 |
| HEARTBEAT     |   4   |   TX/RX   | Heartbeat indicating that the node is working and if it is ready to collect data |
| CLAIM         |   5   |   TX/RX   | Claim master node request                                                        |
| LOG           |   6   |   TX/RX   | Logging message                                                                  |
| LOG_ACK       |   7   |    TX     | Logging message received acknowledgement                                         |
| VERSION       |   8   |   TX/RX   | Firmware versions                                                                |
| ACK           |   9   |    TX     | Frame acknowledgement                                                            |
| FRAMING_ERROR |  10   |    TX     | Framing error (internal error)                                                   |
| DBG           |  11   |    TX     | Firmware debugging message                                                       |

!!! note

    The frame directions are described in relation to the microcontroller.


## SETTING Frame

The setting frame is used to read or set firmware settings that persist over firmware reboots. There are two payload
structures associated with this frame: Key/Value and Key. A table of settings, their ID's, defaults, and ranges is shown
below:

| Setting        | ID | Default | Minimum |  Maximum   | Description                                                                 |
|:---------------|:--:|:-------:|:-------:|:----------:|:----------------------------------------------------------------------------|
| ID             | 0  |    0    |    1    |   65535    | LoRa node identifier. This must be set before use.                          |
| WAIT_USB_HOST  | 1  |    0    |    0    |     1      | Wait for a USB connection to host computer before transmitting over serial. |
| PANID          | 2  |    0    |    0    |   65535    | Personal Area Network ID.                                                   |
| REPETITION_CNT | 4  |   10    |    1    | 4294967295 | Default for the number of times to send a message over LoRa.                |

!!! note

    All bounds are inclusive.


### Key/Value Payload Structure (TX/RX)

This payload structure is used in 2 different scenarios:

1. To set a persistent firmware setting
2. Firmware indicating what the value of a setting is

The payload is structured as such:

|           |              |              |
|-----------|:------------:|:------------:|
| __Field__ |      id      |    value     |
| __Type__  |  `uint16_t`  |  `uint32_t`  |

* __Payload Size__: 6 bytes

### Key Payload Structure (RX)

This payload structure is used in scenarios where the host is requesting the current value of a setting. The payload is 
structured as such:

|           |            |
|-----------|:----------:|
| __Field__ |     id     |
| __Type__  | `uint16_t` |

* __Payload Size__: 2 bytes

## START Frame

Start frames are used to indicate to the other nodes on the network when to start data collection. This frame consists
of a start time, node id, broadcast flag, sequence count, and packet ID. The payload is structured as such:

|           |           |             |            |           |                |            |
|-----------|:---------:|:-----------:|:----------:|:---------:|:--------------:|:----------:|
| __Field__ |  second   | nanoseconds |     id     | broadcast | sequence count | packet id  |
| __Type__  | `int64_t` | `uint64_t`  | `uint16_t` |  `bool`   |   `uint8_t`    | `uint16_t` |

* __Payload size__: 22 bytes

!!! note "ID Field"

    The ID field can mean different things depending on the direction of the frame. If the frame is being sent, it 
    indicates the source of the start message. When receiving, it indicates the destination ID for the message.

!!! note "Broadcast Field"

    The broadcast field, when receiving, indicates if a message should be broadcasted to all listening nodes or not. If
    it is broadcasting the message, the `id` field is ignored. When sending the frame, it is used to indicate if the
    message was broadcasted or not.

!!! note "Sequence Count and Packet ID"

    Sequence Count and Packet ID are derived from the LoRa packet and are ignored by the microcontroller when receiving 
    this packet.

## LORA_CONFIG Frame

LoRa configuration frames are used to configure the LoRa modem. These configurations do not persist after reboots. The
payload structure is as follows:

|           |            |                 |           |           |             |          |           |                   |                |                   |
|-----------|:----------:|:---------------:|:---------:|:---------:|:-----------:|:--------:|:---------:|:-----------------:|:--------------:|:-----------------:|
| __Field__ | Frequency  | Preamble Length | Bandwidth | Data Rate | Coding Rate | Tx Power | CAD Mode  | CAD Symbol Number | Detection Peak | Detection Minimum |
| __Type__  | `uint32_t` |   `uint16_t`    | `uint8_t` | `uint8_t` |  `uint8_t`  | `int8_t` | `uint8_t` |     `uint8_t`     |   `uint8_t`    |     `uint8_t`     |

| Field             |  Default  | Description                                                     |
|:------------------|:---------:|:----------------------------------------------------------------|
| Frequency         | 915000000 | Frequency in Hz to use for transceiving.                        |
| Preamble Length   |     8     | Length of the preamble.                                         |
| Bandwidth         |     0     | The bandwidth to use for transceiving.                          |
| Data Rate         |    12     | The data-rate to use for transceiving.                          |
| Coding Rate       |     1     | The coding rate to use for transceiving.                        |
| Tx Power          |     4     | TX-power in dBm to use for transmission.                        |
| CAD Mode          |     0     | Channel Activity Detection mode.                                |
| CAD Symbol Number |     0     | Number of symbols used for Channel Activity Detection.          |
| Detection Peak    |     0     | Detection peak threshold (hardware-specific, dimensionless).    |
| Detection Minimum |     0     | Minimum detection threshold (hardware-specific, dimensionless). |

* __Payload size__: 14 bytes

!!! note

    CAD Mode, CAD Symbol Number, Detection Peak, and Detection Minimum are unused (for future use). Zephyr RTOS kernel 
    version v4.4.0 or newer is required for these fields.