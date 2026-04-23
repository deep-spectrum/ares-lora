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

| Field             | Default Configuration | Description                                                     |
|:------------------|:---------------------:|:----------------------------------------------------------------|
| Frequency         |       915000000       | Frequency in Hz to use for transceiving.                        |
| Preamble Length   |           8           | Length of the preamble.                                         |
| Bandwidth         |           0           | The bandwidth to use for transceiving.                          |
| Data Rate         |          12           | The data-rate to use for transceiving.                          |
| Coding Rate       |           1           | The coding rate to use for transceiving.                        |
| Tx Power          |           4           | TX-power in dBm to use for transmission.                        |
| CAD Mode          |           0           | Channel Activity Detection mode.                                |
| CAD Symbol Number |           0           | Number of symbols used for Channel Activity Detection.          |
| Detection Peak    |           0           | Detection peak threshold (hardware-specific, dimensionless).    |
| Detection Minimum |           0           | Minimum detection threshold (hardware-specific, dimensionless). |

* __Payload size__: 14 bytes

!!! note

    CAD Mode, CAD Symbol Number, Detection Peak, and Detection Minimum are unused (for future use). Zephyr RTOS kernel 
    version v4.4.0 or newer is required for these fields.


## LED Frame

LED frames are used to either get the current state/action of the LED or to set a new state/action of the LED. The
payload is structured as follows:

|           |              |
|-----------|:------------:|
| __Field__ | State/Action |
| __Type__  |  `uint8_t`   |

| State/Action | Value | Description                                      |
|:-------------|:-----:|:-------------------------------------------------|
| OFF          |   0   | Turn LED off.                                    |
| ON           |   1   | Turn LED on.                                     |
| BLINK        |   2   | Blink LED at 1 Hz.                               |
| FETCH        |   3   | Retrieve the current LED state/action. (RX only) |

* __Payload size__: 1 byte


## HEARTBEAT Frame

The heartbeat frame is used to serve 2 purposes:

1. Indicate if a node is live
2. Indicate if a node is ready to collect data

The payload is structured as follows:

|           |           |           |            |
|-----------|:---------:|:---------:|:----------:|
| __Field__ |   Flags   | TX Count  |  Node ID   |
| __Type__  | `uint8_t` | `uint8_t` | `uint16_t` |

| Field    | Description                                                       |
|:---------|:------------------------------------------------------------------|
| Flags    | `bit 0`: ready <br> `bit 1`: broadcast <br> `bit 7:2`: Reserved   |
| TX Count | How many times to send the heartbeat message.                     |
| Node ID  | The node ID the message is directed to, or the node ID of source. |

* __Payload size__: 4 bytes

!!! note

    When receiving, if the `broadcast` flag is set, the `Node ID` field is ignored. 

    When transmitting, the `broadcast` flag indicates if the message was broadcasted, and the `Node ID` is the source 
    of the message.


## CLAIM Frame

The claim frame is used to indicate which node is the master node in the network. The payload is structured as follows:

|           |            |
|-----------|:----------:|
| __Field__ |     id     |
| __Type__  | `uint16_t` |

* __Payload size__: 2 bytes

!!! note

    When receiving the frame, `id` is the node id that the message should be directed to. 

    When sending the frame, `id` is the node ID of the message source.

## LOG Frame

Log frames are used to send logging messages to other nodes, either through a broadcast or a direct message. The payload
is structured as follows:

|           |           |            |           |           |                 |            |          |
|-----------|:---------:|:----------:|:---------:|:---------:|:---------------:|:----------:|:--------:|
| __Field__ | broadcast |     id     | tx count  |   part    | number of parts |   log Id   | message  |
| __Type__  |  `bool`   | `uint16_t` | `uint8_t` | `uint8_t` |    `uint8_t`    | `uint16_t` | `char[]` |

* __Payload size__: 8 + `len(message)`

!!! note

    When receiving, if `broadcast` is set to `true`, `id` is ignored. If `broadcast` is set to `false`, then `id` is the
    destination ID.

    When transmitting, the `broadcast` flag indicates if the message received was a broadcast message, and `id` is the
    source ID of the message. `tx count` is ignored.

!!! Warning

    To avoid network congestion, log messages should be kept short and sent rarely.


## LOG_ACK Frame

Log ACK frames are used to indicate that a log message was acknowledged by the node that it was directed to. The 
payload is structured as follows:

|           |           |                 |            |
|-----------|:---------:|:---------------:|:----------:|
| __Field__ |   part    | number of parts |   log id   |
| __Type__  | `uint8_t` |    `uint8_t`    | `uint16_t` |

* __Payload size__: 4 bytes

!!! note

    This frame is expected if the LOG frame sent was directed to another node.

## VERSION Frames

Version frames are used to indicate the firmware versions. The payload is structured as follows:

|           |             |             |                     |
|-----------|:-----------:|:-----------:|:-------------------:|
| __Field__ | App version | NCS version | Zephyr RTOS version |
| __Type__  | `uint32_t`  | `uint32_t`  |     `uint32_t`      |

!!! note

    On reception, the payload is ignored.

## ACK Frame

The ACK frame is one of the ways for the firmware to acknowledge the received frame. Additionally, it provides
an error code to indicate if the previous request from the host succeeded or not. The payload is structured as follows:

|           |            |
|-----------|:----------:|
| __Field__ | error code |
| __Type__  | `int32_t`  |

* __Payload size__: 4 bytes

!!! note

    An error code of 0 indicates success.

## FRAMING_ERROR Frame

The Framing error frame is used by the firmware to indicate if there was an error deserializing or deploying a frame. 
The payload is structured as follows:

|           |            |
|-----------|:----------:|
| __Field__ |   error    |
| __Type__  | `uint32_t` |

| Error           | Value | Description                 |
|:----------------|:-----:|:----------------------------|
| Bad frame       |   0   | Not framed correctly.       |
| Bad type        |   1   | Frame type invalid.         |
| Not implemented |   2   | Frame type not implemented. |

## DBG Frame

The debug frame is used to indicate errors in the firmware that are not related to other frames. The payload is 
structures as follows:

|           |           |
|-----------|:---------:|
| __Field__ |   code    |
| __Type__  | `int32_t` |
