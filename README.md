## Overview

This component is designed to control compatible Hitachi air conditioners using serial H-Link protocol. It serves as a replacement for proprietary cloud-based [SPX-WFGXX cloud adapters](https://www.hitachiaircon.com/ranges/iot-apps-controllers/ac-wifi-adapter-aircloud-home), enabling native Home Assistant climate integration through ESPHome. Tested with Hitachi RAK-25PEC AC.

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

<img width="500" alt="wiring_diagram" src="https://github.com/user-attachments/assets/2cb0cb2e-880b-4b07-accb-a8271d7da15c" />

H-link connector is a JST 6-pin PA-6P-TOP with 2.0 mm pitch. 

<img width="135" alt="image" src="https://github.com/user-attachments/assets/7b9b47dd-26e3-4733-a2ff-2601d4fdb389" />

If you're struggling to find a female connector in your local shop (like I did), you can find a similar 6-pin connector with 2.0mm pitch and do some shaping with a needle file. I managed to adapt a HY2.0 plug:

<img width="360" alt="image" src="https://github.com/user-attachments/assets/c3a940ff-2fc4-4db0-8c6e-d144ddded614" />

My AC unit had more than enough space to fit the dev board inside.

<img width="250" alt="image" src="https://github.com/user-attachments/assets/55a9ab5a-a88e-4778-b0ce-15a5f2e5225c" />
<img width="299" alt="image" src="https://github.com/user-attachments/assets/035ad807-4ec6-48c6-948b-3311f392a0a6" />
<img width="236" alt="image" src="https://github.com/user-attachments/assets/af76570f-3b65-477b-97b3-73b63646a645" />

**Be very careful when working with wiring. Always disconnect the AC from the mains before performing any manipulations. Double-check the pinout and voltages to prevent damage to the electronics.**

## ESPHome configuration

```yml
esphome:
  name: "hitachi-ac"

esp32:
  board: XXXX # Put the valid board.
  framework:
    type: esp-idf

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
      ref: 2025.5.1
    components: [hlink_ac]

climate:
  - platform: hlink_ac
    name: "SNXXXXXX"
    hvac_actions: true # Set to false if you don't need HVAC actions.
    supported_presets: # Presets are disabled by default. Remove this if your AC does not support Leave Home mode.
      - AWAY

switch:
  - platform: hlink_ac
    remote_lock:
      name: Remote lock
    beeper:
      name: Beeper

sensor:
  - platform: hlink_ac
    outdoor_temperature:
      name: Outdoor temperature

text_sensor:
  - platform: hlink_ac
    model_name:
      name: Model

number:
  - platform: hlink_ac
    auto_target_temperature_offset:
      name: Auto mode temperature offset
```

without additional configuration the `hlink_ac` climate device provides all features supported by h-link protocol. If your device does not support some of the climate traits - you could adjust the esphome configuration explicitly:

```yml
climate:
  - platform: hlink_ac
    name: "SNXXXXXX"
    supported_modes:
      - "OFF"
      - COOL
    supported_swing_modes:
      - "OFF"
    supported_fan_modes:
      - AUTO
      - LOW
      - HIGH
    visual:
      min_temperature: 16.0
      max_temperature: 28.0
```

### Supported features:
1. Climate
    - HVAC mode:
      - `OFF`
      - `HEAT`
      - `COOL`
      - `DRY`
      - `FAN_ONLY`
      - `AUTO`
    - Fan mode:
      - `QUIET`
      - `LOW`
      - `MEDIUM`
      - `HIGH`
      - `AUTO`
    - Swing mode:
      - `OFF`
      - `VERTICAL`
    - HVAC actions:
      - `OFF`
      - `COOLING`
      - `HEATING`
      - `DRYING`
      - `FAN`
    - Presets:
      - `AWAY`  
2. Switch
    - Remote IR control lock
    - Beeper sounds
3. Sensor
    - Outdoor temperature
4. Text sensor
    - Model name
    - Debug
    - Debug discovery
5. Number
    - Temperature offset in auto mode

## H-link protocol reverse engineering

The H-link specifications are not publicly available, and this component was developed using reverse-engineered data. As a result, it may not cover all possible scenarios and combinations of features offered by different Hitachi climate devices.

If you are interested in exploring the protocol communication on your own, this component provides several text sensors to help monitor H-link addresses dynamically. 
For instance, you can add multiple `debug` text sensors that will be polled repeatedly:

```yaml
text_sensor:
  - platform: hlink_ac
    debug:
      name: P0005
      address: 0x0005
  - platform: hlink_ac
    debug:
      name: P0201
      address: 0x0201
```

Each sensor sends an `MT P=address C=XXXX` request. If the unit returns an `OK` response with a payload, it will be rendered as a text sensor value. For example, the address `0201` most likely returns [error codes](https://github.com/lumixen/esphome-hlink-ac/blob/main/docs/hlink_alarm_codes.csv) if something is wrong with the AC. However, I haven't yet seen reliable proof to add it as an established sensor (fortunately I guess). Debug sensors can help monitor unknown addresses and their behavior throughout the Hitachi unit lifecycle.

Another helpful debug text sensor is called `debug_discovery`. It repeatedly scans the entire range of addresses (0-65535) and prints every non-NG response as a text sensor value (e.g., `0001:8010`/`0304:00000000`/`0302:00`), where the value before the colon is the polled address (P=XXXX), and the value after the colon is the response from the AC. The full range scan takes more than a few hours.

```yaml
text_sensor:
  - platform: hlink_ac
    debug_discovery:
      name: H-link addresses scanner
```

Debug sensors can be paired with the `hlink_ac.send_hlink_cmd` action, which allows you to directly send `ST P=address,value C=XXXX` control frames to AC. Below is an example of an esphome configuration that connects to an MQTT broker and sends an hlink command upon receiving a JSON MQTT message `{"address":"XXXX","data":"DD" | "DDDD"}` in the `hlink_ac/send_hlink_frame` topic:
```yaml
mqtt:
  broker: 1.1.1.1
  ...
  on_json_message:
    topic: hlink_ac/send_hlink_frame
    then:
      - hlink_ac.send_hlink_cmd:
          address: !lambda |-
            if (x.containsKey("address")) {
              return x["address"];
            }
            return "";
          data: !lambda |-
            if (x.containsKey("data")) {
              return x["data"];
            }
            return "";
```

H-link UART serial communication could be monitored using this snippet:

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

## Building locally

Project includes a test [dev configurations](build/) that can be used for compilation.
Run from the project root folder (docker is required):
```bash
cd build/
./compile
```

## Credits

- Florian did a fantastic detective investigation to reverse engineer H-Link connection in his [Let me control you: Hitachi air conditioner](https://hackaday.io/project/168959-let-me-control-you-hitachi-air-conditioner) hackaday project.
- More protocol sniffing in Vince's [hackaday project](https://hackaday.io/project/179797-hitachi-hvac-controler-for-homeassistant-esp8266)
- [hi-arduino project](https://github.com/farom57/hi-arduino/)

## Hardware implementation examples
- [RAS-70YHA2 by shardshunt](https://github.com/shardshunt/H-Link-Docks)
