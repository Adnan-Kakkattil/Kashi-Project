# Required Components List — Site Safety Monitor

## Core components (required)

| Item | Function | Qty | Est. cost (SGD) |
|------|----------|-----|-----------------|
| ESP32-S3 Dev Board | Main microcontroller (Wi-Fi, I2C, I2S, ADC, UART) | 1 | $25 |
| SHT31 | Temperature & humidity | 1 | $15 |
| MQ-2 | Gas / smoke / combustible gases | 1 | $15 |
| MQ-7 | Carbon monoxide (CO) | 1 | $15 |
| INMP441 | Environmental noise (I2S mic) | 1 | $10 |
| OLED Display (SSD1306 0.96") | Local sensor readout | 1 | $20 |
| Buzzer | Audible warning | 1 | — |
| LEDs (+ resistors) | Visual warning / status | 2–3 | $10 (buzzer + LEDs) |
| Enclosure & wiring | Breadboard/PCB, jumper wires, case | 1 set | $15 |
| **Core total** | | | **~$125–150** |

## Optional components (enhancements)

| Item | Function | Qty | Est. cost (SGD) |
|------|----------|-----|-----------------|
| PMS5003 | Dust / PM1.0, PM2.5, PM10 | 1 | $20 |
| Flame sensor (digital DO) | Fire / flame detection | 1 | $10 |
| Speaker module | Voice / tone hazard alerts | 1 | $10 |
| **Optional total** | | | **~$40–50** |

## Power & interconnect (supporting parts)

| Item | Notes |
|------|--------|
| 5 V supply / USB power | ESP32 USB; MQ heaters & PMS5003 often need 5 V |
| 3.3 V rail | SHT31, OLED, INMP441 |
| Common GND | Required across all modules |
| Dupont / jumper wires | Male–female as needed |
| Breadboard or perfboard | Prototype assembly |

## Software / services (no hardware cost)

| Component | Role |
|-----------|------|
| Arduino IDE + ESP32 board package | Firmware build & flash |
| Mosquitto MQTT broker | Message bus (`site/safety/...`) |
| Node-RED + Dashboard | Live monitoring UI |
| Telegram Bot (BotFather) | Remote WARNING notifications |
| Docker Desktop (recommended) | One-command Mosquitto + Node-RED |

## Pin summary (core wiring)

| Component | ESP32-S3 pins |
|-----------|---------------|
| SHT31 / OLED (I2C) | SDA **21**, SCL **22** |
| MQ-2 | GPIO **1** (ADC) |
| MQ-7 | GPIO **2** (ADC) |
| INMP441 | WS **18**, SCK **19**, SD **23** |
| LED | GPIO **4** |
| Buzzer | GPIO **5** |
| PMS5003 (optional) | RX **16**, TX **17** |
| Flame (optional) | GPIO **6** |
| Speaker (optional) | GPIO **25** |

Full detail: [wiring/pinout.md](../wiring/pinout.md)

## Bill of materials checklist

- [ ] ESP32-S3
- [ ] SHT31
- [ ] MQ-2
- [ ] MQ-7
- [ ] INMP441
- [ ] OLED SSD1306
- [ ] Buzzer + LEDs + resistors
- [ ] Enclosure / wiring / 5 V supply
- [ ] (Optional) PMS5003
- [ ] (Optional) Flame sensor
- [ ] (Optional) Speaker

See also: image `assets/required-components-list.png`
