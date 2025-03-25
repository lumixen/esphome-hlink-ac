## Overview

This component is designed to control compatible Hitachi AC units using the serial H-Link protocol. It serves as a replacement for proprietary cloud-based [SPX-WFGXX cloud adapters](https://www.hitachiaircon.com/ranges/iot-apps-controllers/ac-wifi-adapter-aircloud-home), enabling native Home Assistant climate integration through ESPHome.

## Module hardware

For prototyping I used Lolin D32 ESP32 dev board. 

H-Link uses 5V logical levels and exposes 12V power lane, thus we have convert a 12V power lane to 5V for ESP dev board and use a 3.3V<->5V logical level shifter for Tx/Rx lanes. 

<img width="350" alt="image" src="https://github.com/user-attachments/assets/fbedf5c1-f7b6-42a3-8e0d-7b35d1b10b6c" />

An example of wiring diagram with cheapo aliexpress components that worked for me:

## ESPHome configuration example

```yml
esphome:
  name: "hitachi-ac"

esp32:
  board: XXXX
  framework:
    type: arduino

uart:
  id: hitachi_bus
  tx_pin: GPIOXX
  rx_pin: GPIOXX
  baud_rate: 9600
  parity: ODD

external_components:
  - source:
      type: git
      url: https://github.com/lumixen/esphome-hlink-ac.git
      ref: main
    components: [hlink_ac]
    refresh: 0s

climate:
  - platform: hlink_ac
    name: "SNXXXXXX"
```

## Building component

Project includes a test [dev configuration](hlink.yml) that can be used for compilation.
Run from the project root folder (docker is required):
```bash
docker run --rm -v "$(pwd)":/config -it esphome/esphome compile hlink.yml
```

## Credits

- Florian did a fantastic detective investigation to reverse engineer H-Link connection in his [Let me control you: Hitachi air conditioner](https://hackaday.io/project/168959-let-me-control-you-hitachi-air-conditioner) hackerday project.
- More protocol sniffing in Vince's [hackerday project](https://hackaday.io/project/179797-hitachi-hvac-controler-for-homeassistant-esp8266)
