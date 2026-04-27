This page is a brief description of the firmware, toolchain used, hardware platforms used, and how to build the 
firmware.

## Firmware

The firmware for Ares-LoRa is built to be a stateless communication transport. It communicates with the host computer
over USB. The firmware acknowledges all messages sent to it from the host computer, but does not expect acknowledgements
from the host computer when sending events. When communicating over LoRa, the firmware will send the message over LoRa
a specified amount of times and will respond to the driver once done. When receiving events over LoRa, the firmware
will determine if the message was a broadcast message or if the message was intended for that specific node, and it
will forward the message to the serial driver. If the message was not broadcasted nor intended for that node, it will
ignore the message. The only case where the node sends acknowledge messages is when it receives log messages directed
to it.

### Architecture

The firmware is composed of two threads: The Serial message handler and the LoRa message handler. These threads
sleep for most of the time, and they only wake up when there is work available.

## Toolchain

The toolchain used is nRF Connect v3.2.2. This toolchain can be installed either by 
[VS Garbage](https://code.visualstudio.com/){:target="_blank"} or by using 
[nRF Util](https://www.nordicsemi.com/Products/Development-tools/nRF-Util){:target="_blank"}. Please follow 
[Nordic's guide](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/installation/install_ncs.html){:target="_blank"}
for installing NCS.

## Hardware Platforms

These are the hardware platforms used:

| Module/Board        | Microcontroller | LoRa Module    |
|:--------------------|:----------------|:---------------|
| RAKwireless RAK4631 | Nordic nRF52840 | Semtech SX1262 |

## How to build

