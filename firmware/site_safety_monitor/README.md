# ESP32-S3 firmware

Arduino sketch that samples sensors every 2 seconds, evaluates thresholds, drives local alarms/OLED, and publishes MQTT telemetry.

## Libraries

Install via Arduino Library Manager:

- Adafruit SHT31 Library
- Adafruit SSD1306
- Adafruit GFX Library
- PubSubClient (Nick O'Leary)
- ArduinoJson (Benoit Blanchon) v6 or v7

## Config

```bash
copy config.h.example config.h
```

Edit Wi-Fi and MQTT settings. `config.h` is gitignored.

## Board settings (typical)

- Board: ESP32S3 Dev Module
- USB CDC On Boot: Enabled (if using native USB serial)
- Upload speed: 921600 (or lower if unstable)

## Feature flags (`config.h`)

| Macro | Default | Purpose |
|-------|---------|---------|
| `ENABLE_PMS5003` | 1 | Dust sensor UART |
| `ENABLE_FLAME` | 1 | Digital flame DO |
| `ENABLE_VOICE` | 0 | Speaker beep pattern on danger |
| `ENABLE_OLED` | 1 | SSD1306 local display |
