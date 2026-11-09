# Implementation Diagram — Site Safety Monitor

## System implementation overview

```text
┌──────────────────────────────────────────────────────────────────────────┐
│                     SITE SAFETY MONITOR — IMPLEMENTATION                 │
└──────────────────────────────────────────────────────────────────────────┘

  ┌─────────────────────┐
  │   SENSOR LAYER      │
  │  SHT31  (Temp/Hum)  │
  │  MQ-2   (Gas/Smoke) │
  │  MQ-7   (CO)        │
  │  INMP441 (Noise)    │
  │  [Opt] PMS5003 Dust │
  │  [Opt] Flame Sensor │
  └──────────┬──────────┘
             │ GPIO / I2C / I2S / UART
             ▼
  ┌─────────────────────┐         ┌─────────────────────┐
  │  PROCESSING LAYER   │────────▶│   LOCAL OUTPUTS     │
  │     ESP32-S3        │         │  OLED Display       │
  │  Read → Process →   │         │  LED Alert          │
  │  Threshold check    │         │  Buzzer Alarm       │
  │                     │         │  [Opt] Speaker      │
  └──────────┬──────────┘         └─────────────────────┘
             │ Wi-Fi
             ▼
  ┌─────────────────────┐
  │ COMMUNICATION LAYER │
  │  MQTT Broker        │
  │  (Mosquitto :1883)  │
  │  Topics:            │
  │  site/safety/...    │
  └──────────┬──────────┘
             │
             ▼
  ┌─────────────────────┐         ┌─────────────────────┐
  │  MONITORING LAYER   │────────▶│  ALERT CHANNEL      │
  │  Node-RED Dashboard │         │  Telegram Bot API   │
  │  Gauges + Event Log │         │  WARNING messages   │
  │  SAFE / CAUTION /   │         └─────────────────────┘
  │  DANGER indicators  │
  └─────────────────────┘
```

## Implementation sequence

1. **Wire hardware** — sensors, OLED, LED, buzzer to ESP32-S3 ([pinout](../wiring/pinout.md)).
2. **Flash firmware** — `firmware/site_safety_monitor/` with Wi-Fi + MQTT config.
3. **Start broker & dashboard** — `docker compose up -d` (Mosquitto + Node-RED).
4. **Import / confirm flow** — `node-red/flows.json`, set Telegram token + chat ID.
5. **Verify** — live gauges update; threshold breach → local alarm + Telegram.

## Data path

```text
Sensors → ESP32-S3 sample (2 s) → evaluate thresholds
       → publish MQTT telemetry/alert
       → Node-RED UI update
       → if CAUTION/DANGER → Telegram notification
```

## Software stack used in implementation

| Layer | Technology |
|-------|------------|
| Embedded | Arduino IDE, ESP32-S3 core |
| Messaging | MQTT (Mosquitto) |
| Dashboard | Node-RED + node-red-dashboard |
| Notifications | Telegram Bot API |
| Optional sim | `scripts/simulate-telemetry.py` |

See also: [setup-guide.md](setup-guide.md) · image: `assets/implementation-diagram.png`
