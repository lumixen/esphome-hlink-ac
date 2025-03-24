## Overview

This component is designed to control compatible Hitachi AC units using the serial H-Link protocol. It serves as a replacement for proprietary cloud-based SPX-WFGXX adapters, enabling native Home Assistant climate integration.

## Module hardware

For prototyping I used Lolin D32 EPS32 dev board. 

H-Link devices use 5V logical levels and expose 12V power lane, thus we have convert a 12V power lane to 5V for ESP dev board and use a 3.3V<->5V logical level shifter for Tx/Rx lanes. 

<img width="350" alt="image" src="https://github.com/user-attachments/assets/fbedf5c1-f7b6-42a3-8e0d-7b35d1b10b6c" />

An example of wiring diagram that worked for me:

## Building component

This project includes a test [dev configuration](hlink.yml) that can be used for compilation.
Run this from the project root folder (docker is required):
```bash
docker run --rm -v "$(pwd)":/config -it esphome/esphome compile hlink.yml
```

## Credits

- Florian did a fantastic detective investigation to reverse engineer H-Link connection in his [Let me control you: Hitachi air conditioner](https://hackaday.io/project/168959-let-me-control-you-hitachi-air-conditioner) hackerday project.
- More protocol sniffing in Vince's [hackerday project](https://hackaday.io/project/179797-hitachi-hvac-controler-for-homeassistant-esp8266)
