# Node-RED dashboard

Import `flows.json` into Node-RED (or use Docker Compose, which mounts it automatically).

## Required palette nodes

```bash
npm install node-red-dashboard node-red-node-ui-table
```

## Configure Telegram

1. Open the **Set bot token / chat id** function node.
2. Replace `YOUR_TELEGRAM_BOT_TOKEN` and `YOUR_TELEGRAM_CHAT_ID`.
3. Click **Deploy**.

Alerts arrive on MQTT topic `site/safety/alert` and are forwarded to Telegram.

## Broker

Default broker host in the flow is `localhost` (correct when Node-RED and Mosquitto run via the same Docker Compose stack or on the same PC).

If Node-RED runs elsewhere, edit the **Local Mosquitto** broker node host.
