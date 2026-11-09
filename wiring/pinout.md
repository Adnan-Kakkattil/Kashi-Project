# Hardware pinout (ESP32-S3)

Based on the project circuit diagram. MQ gas sensors use **ADC1-capable** pins on ESP32-S3 (GPIO 1–10). The original report listed GPIO34/35 (classic ESP32 ADC); those pins are remapped here.

| Function | Module pin | ESP32-S3 GPIO |
|----------|------------|---------------|
| I2C SDA | SHT31 / OLED SDA | **21** |
| I2C SCL | SHT31 / OLED SCL | **22** |
| MQ-2 analog | A0 | **1** (was 34 in report) |
| MQ-7 analog | A0 | **2** (was 35 in report) |
| INMP441 WS | WS | **18** |
| INMP441 SCK | SCK | **19** |
| INMP441 SD | SD | **23** |
| Status LED | anode via resistor | **4** |
| Buzzer | + | **5** |
| PMS5003 RX | sensor TX → ESP RX | **16** |
| PMS5003 TX | sensor RX ← ESP TX | **17** |
| Speaker (optional) | signal | **25** |
| Flame sensor (optional) | DO | **6** (active LOW) |

## Power

- Share a **common GND** across ESP32-S3 and all modules.
- SHT31 / OLED / INMP441: **3.3 V**
- MQ-2 / MQ-7 heater: typically **5 V** (signal still to ESP ADC)
- PMS5003: **5 V**

## I2C addresses

- SHT31: `0x44` (or `0x45`)
- SSD1306 OLED: `0x3C`
