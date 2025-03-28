## Overview

This component is designed to control compatible Hitachi air conditioners using the serial H-Link protocol. It serves as a replacement for proprietary cloud-based [SPX-WFGXX cloud adapters](https://www.hitachiaircon.com/ranges/iot-apps-controllers/ac-wifi-adapter-aircloud-home), enabling native Home Assistant climate integration through ESPHome. Tested with Hitachi RAK-25PEC AC.

## H-link protocol

H-link is a serial protocol designed to enable communication between climate units and external adapters, such as a central station managing multiple climate devices in commercial buildings or SPX-WFGXX cloud adapters. It allows to read the status of a climate unit and send commands to control it.

The protocol supports two types of communication frames:
1. Status inquiry from adapter, initiated with the `MT` prefix:
```
>> MT P=XXXX C=YYYY
<< OK P=XXXX C=YYYY
```
where `MT P=XXXX` is 16-bit numerical command and `C=YYYY` is a 16-bit XOR checksum. AC unit returns `OK P=XXXX`, where `XXXX` is the dynamic-length value of the requested "feature" (e.g. power state, climate mode, swing mode etc). 

2. Status change request, initiated with the `ST` prefix:
```
>> ST P=XXXX,XX(XX) C=YYYY
<< OK
``` 
where `P=XXXX,XX(XX)` specifies the function to modify and the new value, `C=YYYY` - 16-bit XOR checksum. The response `OK` confirms successful execution.

## Hardware

For PoC project I used the Lolin D32 ESP32 dev board. 

H-Link operates at 5V logic levels and provides a 12V power lane. Therefore, we need to step down the 12V power lane to 5V for the ESP dev board 5V input and use a 3.3V-to-5V logic level shifter for the Tx/Rx communication lines:

<img width="350" alt="hlink_connector" src="https://github.com/user-attachments/assets/fbedf5c1-f7b6-42a3-8e0d-7b35d1b10b6c" />

An example of wiring diagram with cheapo aliexpress building blocks that worked for me:

<img width="500" alt="wiring_diagram" src="https://github.com/user-attachments/assets/f42c1574-32fe-48b3-9542-019c560d525f" />

H-link connector is a 6-pin JST PA with 2.0 mm pitch. 

<img width="135" alt="image" src="https://github.com/user-attachments/assets/7b9b47dd-26e3-4733-a2ff-2601d4fdb389" />

If you're struggling to find a female connector in your local shop (like I did), you can find a similar 6-pin connector with 2.0mm pitch and do some shaping with a needle file. I managed to adapt a HY2.0 plug:

<img width="360" alt="image" src="https://github.com/user-attachments/assets/c3a940ff-2fc4-4db0-8c6e-d144ddded614" />

My AC unit had more than enough space to fit the dev board inside.

<img width="250" alt="image" src="https://github.com/user-attachments/assets/55a9ab5a-a88e-4778-b0ce-15a5f2e5225c" />
<img width="300" alt="image" src="https://github.com/user-attachments/assets/035ad807-4ec6-48c6-948b-3311f392a0a6" />

## ESPHome configuration

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

climate:
  - platform: hlink_ac
    name: "SNXXXXXX"
```

Supported climate traits:
1. Climate mode:
 - `OFF`
 - `HEAT`
 - `COOL`
 - `DRY`
2. Fan mode:
 - `QUIET`
 - `LOW`
 - `MEDIUM`
 - `HIGH`
 - `AUTO`
3. Swing mode:
 - `OFF`
 - `VERTICAL`

<img width="300" alt="image" src="https://github.com/user-attachments/assets/2d6c29c3-ea18-4a45-8814-2f24d88ea3e2" />

## Building locally

Project includes a test [dev configuration](hlink.yml) that can be used for compilation.
Run from the project root folder (docker is required):
```bash
docker run --rm -v "$(pwd)":/config -it esphome/esphome compile hlink.yml
```

## Debugging serial communication

```yaml
uart:
  id: hitachi_bus
  tx_pin: GPIOXX
  rx_pin: GPIOXX
  baud_rate: 9600
  parity: ODD
  debug:
    direction: BOTH
    dummy_receiver: false
    after:
      delimiter: "\n"
    sequence:
      - lambda: UARTDebug::log_string(direction, bytes);
```

## Credits

- Florian did a fantastic detective investigation to reverse engineer H-Link connection in his [Let me control you: Hitachi air conditioner](https://hackaday.io/project/168959-let-me-control-you-hitachi-air-conditioner) hackerday project.
- More protocol sniffing in Vince's [hackerday project](https://hackaday.io/project/179797-hitachi-hvac-controler-for-homeassistant-esp8266)
