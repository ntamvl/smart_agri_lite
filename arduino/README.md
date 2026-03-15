# Arduino Code

## Libraries
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
- [PubSubClient](https://github.com/knolleary/pubsubclient)
- ESP8266

**Additional Board Manager for Arduino IDE**
```
https://arduino.esp8266.com/stable/package_esp8266com_index.json
https://espressif.github.io/arduino-esp32/package_esp32_index.json
https://raw.githubusercontent.com/SpacehuhnTech/arduino/main/package_spacehuhn_index.json
```

## Hướng dẫn cài đặt và upload code Arduino
Download và cài đặt Arduino IDE https://www.arduino.cc/en/software

Các bước cấu hình và upload code Arduino:
1. Cài đặt board ESP8266
2. Menu --> Tools --> Board --> esp8266 --> NodeMCU 1.0 (ESP-12E Module)
3. Menu --> Tools --> Port --> chọn cổng COM phù hợp
4. Menu --> Tools --> Erase Flash --> "All Flash Contents"
5. Menu --> Sketch --> Upload

## Run Smart Agri Web App

**Run with docker**
```bash
./start_dev_docker.sh

# OR

docker-compose -f docker-compose.dev.yml up
```
