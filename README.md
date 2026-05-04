# ota-firmware-protocol
This repository contains code for an ESP32 WiFI enabled over-the-air (OTA) protocol for a mesh network of smart home nodes. The appeal of this protocol is the ability for each node to detect firmware updates depending on the node type and automatically install these updates over WiFi. 

Each microcontroller node can serve a unique purpose and listen for updated firmware from the host server. Upon an update being posted to the server, the nodes assigned that specific role will initiate an OTA update automatically.

Going forward, further experimentation is underway on a protocol that uses LoRaWANN instead of WiFi for a protocol that can be served over remote areas where WiFi is unavailable. This will be accomplished with a unique TX/RX protocol involving chunking firmware binary packets and custom board design. 
