## Hardware

For prototyping I used Lolin D32 EPS32 dev board. 

As H-Link uses 5V logical levels and exposes 12V power lane, thus we have convert a 12V power lane to 5V and use a 3.3V<->5V logical level shifter for Tx/Rx lanes. An example of wiring diagram that worked well for me:

## Building 

This project includes a test [dev configuration](hlink.yml) that can be used for compilation.
Run this from the project root folder (installed docker is required):
```bash
docker run --rm -v "$(pwd)":/config -it esphome/esphome compile hlink.yml
```

## Credits

- Florian did a fantastic detective investigation to reverse engineer H-Link connection in his [hackerday project](https://hackaday.io/project/168959-let-me-control-you-hitachi-air-conditioner).
- Extra details on protocol communication in Vince's [hackerday project](https://hackaday.io/project/179797-hitachi-hvac-controler-for-homeassistant-esp8266)