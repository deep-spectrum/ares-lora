# Ares Packets

The firmware communicates with other nodes in the area with packets called "Ares Packets." There are 2 types
of packets: broadcast and direct. Broadcast packets are used in cases where a message needs to be sent to all the nodes
in the area, while direct packets are used to send a message to a specific node in the network. The packets are
structured as follows:

=== "Broadcast Packets"

    | Field            | Header | Payload Length | Packet Type | Packet ID | Sequence Count | Pan ID | Source ID | Payload Type | Payload | CRC16 | Footer |
    |:-----------------|:------:|:--------------:|:-----------:|:---------:|:--------------:|:------:|:---------:|:------------:|:-------:|:-----:|:------:|
    | __Size (bytes)__ |   2    |       2        |      1      |     2     |       1        |   2    |     2     |      1       | 0-65535 |   2   |   2    |
    | __Value__        |   AR   |       n        |      0      |    ...    |      ...       |  ...   |    ...    |     ...      |   ...   |  ...  |   ES   |


=== "Direct Packets"

    | Field            | Header | Payload Length | Packet Type | Packet ID | Sequence Count | Pan ID | Source ID | Destination ID | Payload Type | Payload | CRC16 | Footer |
    |:-----------------|:------:|:--------------:|:-----------:|:---------:|:--------------:|:------:|:---------:|:--------------:|:------------:|:-------:|:-----:|:------:|
    | __Size (bytes)__ |   2    |       2        |      1      |     2     |       1        |   2    |     2     |       2        |      1       | 0-65535 |   2   |   2    |
    | __Value__        |   AR   |       n        |      0      |    ...    |      ...       |  ...   |    ...    |      ...       |     ...      |   ...   |  ...  |   ES   |


There are a variety of payloads that these packets can send which are described below:

| Type      | Value | Packet Types Allowed | Description                    |
|:----------|:-----:|:--------------------:|:-------------------------------|
| START     |   0   |   Broadcast/Direct   | Start time.                    |
| HEARTBEAT |   1   |   Broadcast/Direct   | Heartbeat from slave nodes.    |
| CLAIM     |   2   |        Direct        | Claim master message.          |
| LOG       |   3   |   Broadcast/Direct   | Send log message over network. |
| LOG_ACK   |   4   |        Direct        | Log message acknowledgement.   |

## START Payload Type

Start payloads are used to indicate the start time for data collection. They can either be directed at a certain
node or broadcasted to all the nodes in the area. The payload is structured as follows:

|           |           |             |
|-----------|:---------:|:-----------:|
| __Field__ |  seconds  | nanoseconds |
| __Type__  | `int64_t` | `uint64_t`  |

* __Payload size__: 16 bytes

## HEARTBEAT Payload Type:

Heartbeat payloads are used to indicate that a node is online and whether it is ready to start collecting data or not.
They can either be directed at a certain node or broadcasted to all the nodes in the area. The payload is structured as 
follows:

|           |        |
|-----------|:------:|
| __Field__ | ready  |
| __Type__  | `bool` |

* __Payload size__: 1 byte

## CLAIM Payload Type

Claim payloads are used to indicate to a certain node that the sender is the master node of the network. This message
must be a direct message. The payload for this type is empty.

## LOG Payload Type

Log payloads are used to send logging messages over the network. They can be either be directed at a certain node or
broadcasted to all the nodes in the area. The payload is structured as follows:

|           |           |           |            |         |
|-----------|:---------:|:---------:|:----------:|:-------:|
| __Field__ |   part    | num_parts |   log_id   | message |
| __Type__  | `uint8_t` | `uint8_t` | `uint16_t` | char[]  |

* __Payload size__: 4 + `len(message)` bytes

## LOG_ACK Payload Type

Log acknowledgement payloads are used to acknowledge logging messages that were directed to a certain node. This message
must be a direct message. The payload is structured as follows:

|           |           |           |            |
|-----------|:---------:|:---------:|:----------:|
| __Field__ |   part    | num_parts |   log_id   |
| __Type__  | `uint8_t` | `uint8_t` | `uint16_t` |

* __Payload size__: 4 bytes
