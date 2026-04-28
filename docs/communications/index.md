# Communications

Communication with an Ares LoRa node and the host computer is carried out over USB. Node-to-node communication
is carried out over LoRa. The USB uses [Ares Frames](ares-frame.md) to communicate while the LoRa use 
[Ares Packets](ares-packet.md) to communicate.

## Serial Protocol

When communicating over serial, there are two things that can happen:

1. The host sends a frame to the node
2. The node sends the host an event notification

In the event that the host sends a frame to the node, the node will respond or acknowledge to the frame it received
after running its routine to process it. On the other hand, the node can send the host event notifications, which can
be sporadic. The node does not expect a response from the host when sending these event notifications.

|                                                                    |                                                           |
|:------------------------------------------------------------------:|:---------------------------------------------------------:|
| ![Frame Host to Node Communication](assets/frame-host-request.png) | ![Frame Event Notification](assets/frame-event-notif.png) |

## LoRa Protocol

When a node is communicating over LoRa, it will send the same packet on the network the specified amount of times (whether
it is specified by the host or just loaded from the settings). There is one exception, when sending logs to a specific
node. When sending logs to a specific node, the destination node is expected to acknowledge the packet. If it doesn't
get acknowledged, then the driver will retry or throw an error after it has tried the specified number of times.

|                                                                        |                                               |
|:----------------------------------------------------------------------:|:---------------------------------------------:|
|        ![Packet Normal Communication](assets/packet-normal.png)        | ![Packet Directed Log](assets/packet-log.png) |
