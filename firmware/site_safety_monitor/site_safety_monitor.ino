/**
 * Site Safety Monitor — ESP32-S3 firmware
 * Sensors: SHT31, MQ-2, MQ-7, INMP441 (+ optional PMS5003, flame)
 * Publishes JSON over MQTT for Node-RED dashboard + Telegram alerts.
 *
 * Board: ESP32-S3 Dev Module
 * Arduino IDE libraries:
 *   - Adafruit SHT31 Library
 *   - Adafruit SSD1306 (+ Adafruit GFX)
 *   - PubSubClient
 *   - ArduinoJson (v6+)
 */

#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <driver/i2s.h>
#include <math.h>
#include "config.h"

// ---------------------------------------------------------------------------
// Pin map (ESP32-S3). MQ pins use ADC1-capable GPIOs (1–10).
// Report diagram listed GPIO34/35 (classic ESP32 ADC); remapped for S3.
// ---------------------------------------------------------------------------
static const int PIN_I2C_SDA   = 21;
static const int PIN_I2C_SCL   = 22;
static const int PIN_MQ2       = 1;   // ADC
static const int PIN_MQ7       = 2;   // ADC
static const int PIN_I2S_WS    = 18;
static const int PIN_I2S_SCK   = 19;
static const int PIN_I2S_SD    = 23;
static const int PIN_LED       = 4;
static const int PIN_BUZZER    = 5;
static const int PIN_PMS_RX    = 16;  // ESP32 RX <- PMS TX
static const int PIN_PMS_TX    = 17;  // ESP32 TX -> PMS RX
static const int PIN_SPEAKER   = 25;
static const int PIN_FLAME     = 6;   // digital, active LOW common modules

// Thresholds (align with config/thresholds.json)
static const float TEMP_CAUTION_C   = 32.0f;
static const float TEMP_DANGER_C    = 38.0f;
static const float HUM_CAUTION_PCT  = 80.0f;
static const int   MQ2_CAUTION      = 1200;
static const int   MQ2_DANGER       = 2000;
static const int   MQ7_CAUTION      = 800;
static const int   MQ7_DANGER       = 1500;
static const float NOISE_CAUTION_DB = 85.0f;
static const float NOISE_DANGER_DB  = 95.0f;
static const int   PM25_CAUTION     = 55;
static const int   PM25_DANGER      = 150;

static const unsigned long SAMPLE_MS      = 2000;
static const unsigned long MQTT_RETRY_MS  = 5000;
static const unsigned long ALERT_COOLDOWN = 60000;

Adafruit_SHT31 sht31 = Adafruit_SHT31();
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
HardwareSerial PMSSerial(1);

struct Readings {
  float temperature = NAN;
  float humidity = NAN;
  int mq2 = 0;
  int mq7 = 0;
  float noiseDb = 0;
  int pm1 = 0;
  int pm25 = 0;
  int pm10 = 0;
  bool flame = false;
  bool validTemp = false;
};

enum AlertLevel { LEVEL_SAFE = 0, LEVEL_CAUTION = 1, LEVEL_DANGER = 2 };

unsigned long lastSample = 0;
unsigned long lastMqttAttempt = 0;
unsigned long lastAlertSent = 0;
String lastAlertReason = "";

// ---------------------------------------------------------------------------
void setupPins() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_SPEAKER, OUTPUT);
  pinMode(PIN_FLAME, INPUT_PULLUP);
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_BUZZER, LOW);
  analogReadResolution(12);
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi FAILED — will retry in loop");
  }
}

bool setupI2SMic() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num = PIN_I2S_SCK,
    .ws_io_num = PIN_I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = PIN_I2S_SD
  };
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) return false;
  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) return false;
  return true;
}

float readNoiseDb() {
  const int N = 512;
  int32_t samples[N];
  size_t bytesRead = 0;
  if (i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, pdMS_TO_TICKS(100)) != ESP_OK) {
    return 0;
  }
  int count = bytesRead / sizeof(int32_t);
  if (count <= 0) return 0;

  double sumSq = 0;
  for (int i = 0; i < count; i++) {
    int32_t s = samples[i] >> 8; // 24-bit meaningful data in MSB
    sumSq += (double)s * (double)s;
  }
  double rms = sqrt(sumSq / count);
  if (rms < 1.0) rms = 1.0;
  // Rough dB estimate — calibrate against a sound meter for production use
  float db = 20.0f * log10f((float)rms) - 40.0f;
  if (db < 30) db = 30;
  if (db > 120) db = 120;
  return db;
}

bool readPms5003(int &pm1, int &pm25, int &pm10) {
  // Passive mode frame: 0x42 0x4D ... 32 bytes
  while (PMSSerial.available() >= 32) {
    if (PMSSerial.peek() != 0x42) {
      PMSSerial.read();
      continue;
    }
    uint8_t buf[32];
    if (PMSSerial.readBytes(buf, 32) != 32) return false;
    if (buf[0] != 0x42 || buf[1] != 0x4D) return false;
    uint16_t sum = 0;
    for (int i = 0; i < 30; i++) sum += buf[i];
    uint16_t checksum = (buf[30] << 8) | buf[31];
    if (sum != checksum) return false;
    pm1  = (buf[10] << 8) | buf[11];
    pm25 = (buf[12] << 8) | buf[13];
    pm10 = (buf[14] << 8) | buf[15];
    return true;
  }
  return false;
}

void ensureMqtt() {
  if (mqtt.connected()) return;
  if (WiFi.status() != WL_CONNECTED) {
    setupWifi();
    return;
  }
  if (millis() - lastMqttAttempt < MQTT_RETRY_MS) return;
  lastMqttAttempt = millis();

  Serial.print("MQTT connect...");
  bool ok;
  if (strlen(MQTT_USER) > 0) {
    ok = mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
  } else {
    ok = mqtt.connect(MQTT_CLIENT_ID);
  }
  if (ok) {
    Serial.println("OK");
    mqtt.publish((String(MQTT_TOPIC_BASE) + "/status").c_str(), "online", true);
  } else {
    Serial.printf("FAIL rc=%d\n", mqtt.state());
  }
}

AlertLevel evaluate(const Readings &r, String &reason) {
  AlertLevel level = LEVEL_SAFE;
  reason = "";

  auto raise = [&](AlertLevel l, const char *msg) {
    if (l > level) {
      level = l;
      reason = msg;
    }
  };

  if (r.validTemp) {
    if (r.temperature >= TEMP_DANGER_C) raise(LEVEL_DANGER, "High temperature");
    else if (r.temperature >= TEMP_CAUTION_C) raise(LEVEL_CAUTION, "Elevated temperature");
    if (r.humidity >= HUM_CAUTION_PCT) raise(LEVEL_CAUTION, "High humidity");
  }
  if (r.mq2 >= MQ2_DANGER) raise(LEVEL_DANGER, "Smoke / combustible gas");
  else if (r.mq2 >= MQ2_CAUTION) raise(LEVEL_CAUTION, "Elevated gas / smoke");
  if (r.mq7 >= MQ7_DANGER) raise(LEVEL_DANGER, "High carbon monoxide");
  else if (r.mq7 >= MQ7_CAUTION) raise(LEVEL_CAUTION, "Elevated carbon monoxide");
  if (r.noiseDb >= NOISE_DANGER_DB) raise(LEVEL_DANGER, "Dangerous noise level");
  else if (r.noiseDb >= NOISE_CAUTION_DB) raise(LEVEL_CAUTION, "High noise level");

#if ENABLE_PMS5003
  if (r.pm25 >= PM25_DANGER) raise(LEVEL_DANGER, "Hazardous PM2.5 dust");
  else if (r.pm25 >= PM25_CAUTION) raise(LEVEL_CAUTION, "Elevated PM2.5 dust");
#endif
#if ENABLE_FLAME
  if (r.flame) raise(LEVEL_DANGER, "Flame detected");
#endif

  return level;
}

void setLocalAlarm(AlertLevel level) {
  if (level == LEVEL_DANGER) {
    digitalWrite(PIN_LED, HIGH);
    digitalWrite(PIN_BUZZER, HIGH);
  } else if (level == LEVEL_CAUTION) {
    digitalWrite(PIN_LED, (millis() / 400) % 2);
    digitalWrite(PIN_BUZZER, LOW);
  } else {
    digitalWrite(PIN_LED, LOW);
    digitalWrite(PIN_BUZZER, LOW);
  }
}

#if ENABLE_VOICE
void voiceBeepPattern() {
  // Simple tone pattern on GPIO speaker pin (PWM-ish bit-bang)
  for (int i = 0; i < 3; i++) {
    for (int t = 0; t < 200; t++) {
      digitalWrite(PIN_SPEAKER, HIGH);
      delayMicroseconds(500);
      digitalWrite(PIN_SPEAKER, LOW);
      delayMicroseconds(500);
    }
    delay(150);
  }
}
#endif

void updateOled(const Readings &r, AlertLevel level, const String &reason) {
#if ENABLE_OLED
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.println("Site Safety Monitor");
  oled.println("--------------------");
  if (r.validTemp) {
    oled.printf("T:%.1fC  H:%.0f%%\n", r.temperature, r.humidity);
  } else {
    oled.println("T/H: --");
  }
  oled.printf("Gas:%d  CO:%d\n", r.mq2, r.mq7);
  oled.printf("Noise: %.0f dB\n", r.noiseDb);
#if ENABLE_PMS5003
  oled.printf("PM2.5:%d\n", r.pm25);
#endif
  const char *lvl = level == LEVEL_DANGER ? "DANGER" : level == LEVEL_CAUTION ? "CAUTION" : "SAFE";
  oled.printf("Status: %s\n", lvl);
  if (reason.length()) {
    oled.println(reason.substring(0, 20));
  }
  oled.display();
#else
  (void)r; (void)level; (void)reason;
#endif
}

void publishReadings(const Readings &r, AlertLevel level, const String &reason) {
  if (!mqtt.connected()) return;

  StaticJsonDocument<512> doc;
  if (r.validTemp) {
    doc["temperature"] = r.temperature;
    doc["humidity"] = r.humidity;
  }
  doc["mq2"] = r.mq2;
  doc["mq7"] = r.mq7;
  doc["noise_db"] = r.noiseDb;
#if ENABLE_PMS5003
  doc["pm1"] = r.pm1;
  doc["pm25"] = r.pm25;
  doc["pm10"] = r.pm10;
#endif
#if ENABLE_FLAME
  doc["flame"] = r.flame;
#endif
  doc["status"] = level == LEVEL_DANGER ? "DANGER" : level == LEVEL_CAUTION ? "CAUTION" : "SAFE";
  doc["reason"] = reason;
  doc["uptime_s"] = millis() / 1000;

  char payload[512];
  size_t n = serializeJson(doc, payload, sizeof(payload));

  String topic = String(MQTT_TOPIC_BASE) + "/telemetry";
  mqtt.publish(topic.c_str(), payload, n);

  // Individual topics for Node-RED gauges
  mqtt.publish((String(MQTT_TOPIC_BASE) + "/temperature").c_str(),
               r.validTemp ? String(r.temperature, 1).c_str() : "nan");
  mqtt.publish((String(MQTT_TOPIC_BASE) + "/humidity").c_str(),
               r.validTemp ? String(r.humidity, 1).c_str() : "nan");
  mqtt.publish((String(MQTT_TOPIC_BASE) + "/mq2").c_str(), String(r.mq2).c_str());
  mqtt.publish((String(MQTT_TOPIC_BASE) + "/mq7").c_str(), String(r.mq7).c_str());
  mqtt.publish((String(MQTT_TOPIC_BASE) + "/noise").c_str(), String(r.noiseDb, 1).c_str());
  mqtt.publish((String(MQTT_TOPIC_BASE) + "/status").c_str(),
               level == LEVEL_DANGER ? "DANGER" : level == LEVEL_CAUTION ? "CAUTION" : "SAFE");

  if (level >= LEVEL_CAUTION) {
    unsigned long now = millis();
    if (reason != lastAlertReason || now - lastAlertSent > ALERT_COOLDOWN) {
      StaticJsonDocument<256> alert;
      alert["level"] = doc["status"];
      alert["reason"] = reason;
      alert["temperature"] = r.validTemp ? r.temperature : 0;
      alert["mq7"] = r.mq7;
      alert["mq2"] = r.mq2;
      alert["noise_db"] = r.noiseDb;
      char ap[256];
      size_t an = serializeJson(alert, ap, sizeof(ap));
      mqtt.publish((String(MQTT_TOPIC_BASE) + "/alert").c_str(), ap, an);
      lastAlertSent = now;
      lastAlertReason = reason;
#if ENABLE_VOICE
      if (level == LEVEL_DANGER) voiceBeepPattern();
#endif
    }
  }
}

Readings sampleSensors() {
  Readings r;
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    r.temperature = t;
    r.humidity = h;
    r.validTemp = true;
  }
  r.mq2 = analogRead(PIN_MQ2);
  r.mq7 = analogRead(PIN_MQ7);
  r.noiseDb = readNoiseDb();

#if ENABLE_PMS5003
  int p1, p25, p10;
  if (readPms5003(p1, p25, p10)) {
    r.pm1 = p1;
    r.pm25 = p25;
    r.pm10 = p10;
  }
#endif
#if ENABLE_FLAME
  r.flame = (digitalRead(PIN_FLAME) == LOW);
#endif
  return r;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Site Safety Monitor (ESP32-S3) ===");

  setupPins();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  if (!sht31.begin(0x44)) {
    Serial.println("SHT31 not found @0x44 — trying 0x45");
    if (!sht31.begin(0x45)) {
      Serial.println("SHT31 missing — temp/humidity disabled");
    }
  }

#if ENABLE_OLED
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found");
  } else {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0, 0);
    oled.println("Booting...");
    oled.display();
  }
#endif

  if (!setupI2SMic()) {
    Serial.println("I2S mic init failed");
  }

#if ENABLE_PMS5003
  PMSSerial.begin(9600, SERIAL_8N1, PIN_PMS_RX, PIN_PMS_TX);
#endif

  setupWifi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setBufferSize(512);
}

void loop() {
  ensureMqtt();
  if (mqtt.connected()) mqtt.loop();

  if (millis() - lastSample >= SAMPLE_MS) {
    lastSample = millis();
    Readings r = sampleSensors();
    String reason;
    AlertLevel level = evaluate(r, reason);
    setLocalAlarm(level);
    updateOled(r, level, reason);
    publishReadings(r, level, reason);

    Serial.printf("T=%.1f H=%.0f MQ2=%d MQ7=%d dB=%.0f [%s] %s\n",
                  r.temperature, r.humidity, r.mq2, r.mq7, r.noiseDb,
                  level == LEVEL_DANGER ? "DANGER" : level == LEVEL_CAUTION ? "CAUTION" : "SAFE",
                  reason.c_str());
  }

  // Keep caution LED blinking responsive
  if (digitalRead(PIN_BUZZER) == LOW) {
    // re-evaluate blink only when not in solid danger alarm
  }
}
