#!/usr/bin/env python3
"""Publish sample telemetry to MQTT for dashboard testing without hardware.

Usage:
  pip install paho-mqtt
  python scripts/simulate-telemetry.py --host localhost
"""

from __future__ import annotations

import argparse
import json
import math
import time

try:
    import paho.mqtt.client as mqtt
except ImportError as exc:
    raise SystemExit("Install dependency: pip install paho-mqtt") from exc


def main() -> None:
    parser = argparse.ArgumentParser(description="Simulate site safety MQTT telemetry")
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--base", default="site/safety")
    args = parser.parse_args()

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="site-safety-sim")
    client.connect(args.host, args.port, 60)
    client.loop_start()

    t0 = time.time()
    print(f"Publishing to {args.host}:{args.port} base={args.base} (Ctrl+C to stop)")
    try:
        while True:
            elapsed = time.time() - t0
            temperature = 28 + 6 * abs(math.sin(elapsed / 20))
            humidity = 55 + 10 * math.sin(elapsed / 15)
            mq2 = int(400 + 200 * abs(math.sin(elapsed / 8)))
            mq7 = int(300 + 100 * abs(math.cos(elapsed / 9)))
            noise = 60 + 20 * abs(math.sin(elapsed / 5))
            pm25 = int(20 + 15 * abs(math.sin(elapsed / 12)))

            status = "SAFE"
            reason = ""
            if temperature >= 38 or mq2 >= 2000 or mq7 >= 1500 or noise >= 95:
                status = "DANGER"
                reason = "Simulated danger condition"
            elif temperature >= 32 or mq2 >= 1200 or mq7 >= 800 or noise >= 85:
                status = "CAUTION"
                reason = "Simulated caution condition"

            payload = {
                "temperature": round(temperature, 1),
                "humidity": round(humidity, 1),
                "mq2": mq2,
                "mq7": mq7,
                "noise_db": round(noise, 1),
                "pm1": pm25 - 5,
                "pm25": pm25,
                "pm10": pm25 + 10,
                "flame": False,
                "status": status,
                "reason": reason,
                "uptime_s": int(elapsed),
            }

            topic = f"{args.base}/telemetry"
            client.publish(topic, json.dumps(payload))
            client.publish(f"{args.base}/status", status)
            if status != "SAFE":
                client.publish(
                    f"{args.base}/alert",
                    json.dumps(
                        {
                            "level": status,
                            "reason": reason,
                            "temperature": payload["temperature"],
                            "mq2": mq2,
                            "mq7": mq7,
                            "noise_db": payload["noise_db"],
                        }
                    ),
                )
            print(payload)
            time.sleep(2)
    except KeyboardInterrupt:
        print("\nStopped")
    finally:
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()
