# Ares-IQ LoRa Remote Start

## Firmware Build Notes

It was found that the optimization level matters with the firmware. When building for speed,
LoRa stops working. I am not sure why this is, but my guess is that it has to do with either 
the driver itself, or it has to do with the packets (most likely culprit). The one optimization 
level that was found to work was debug.
