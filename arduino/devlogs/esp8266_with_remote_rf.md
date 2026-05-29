# ESP8266 with Remote RF

```cpp
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ========== CẤU HÌNH - SỬA TẠI ĐÂY ==========
const char* WIFI_SSID      = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD  = "YOUR_WIFI_PASSWORD";

const char* MQTT_SERVER    = "192.168.1.100";
const int   MQTT_PORT      = 1883;
const char* MQTT_USER      = "";
const char* MQTT_PASSWORD  = "";
const char* MQTT_CLIENT_ID = "ESP8266_PIN_CTRL";

const unsigned long STATUS_INTERVAL_MS = 60000;
// =================================================

String MQTT_TOPIC_SUB;
String MQTT_TOPIC_STATUS;

ESP8266WebServer server(80);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ── Nguồn điều khiển relay ────────────────────────────────────
enum ControlSource {
  SOURCE_NONE,  // chưa có lệnh nào
  SOURCE_RF,    // do Remote RF điều khiển
  SOURCE_MQTT,  // do MQTT điều khiển
  SOURCE_HTTP   // do HTTP điều khiển
};

// ── Mapping RF (INPUT) ↔ Relay (OUTPUT) ──────────────────────
struct RFRelayMap {
  int rfPin;
  int relayPin;
};

const RFRelayMap RF_RELAY_MAP[] = {
  { 16, 14 },
  {  5, 12 },
  {  4, 13 },
};
const int RF_RELAY_COUNT = sizeof(RF_RELAY_MAP) / sizeof(RF_RELAY_MAP[0]);

int relayPinFromRF(int rfPin) {
  for (int i = 0; i < RF_RELAY_COUNT; i++)
    if (RF_RELAY_MAP[i].rfPin == rfPin) return RF_RELAY_MAP[i].relayPin;
  return -1;
}

int rfPinFromRelay(int relayPin) {
  for (int i = 0; i < RF_RELAY_COUNT; i++)
    if (RF_RELAY_MAP[i].relayPin == relayPin) return RF_RELAY_MAP[i].rfPin;
  return -1;
}

// ── PinState ──────────────────────────────────────────────────
const int VALID_PINS[]     = {0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16};
const int VALID_PINS_COUNT = sizeof(VALID_PINS) / sizeof(VALID_PINS[0]);

struct PinState {
  int           pin;
  int           value;
  bool          active;
  ControlSource source; // ← nguồn điều khiển
};

PinState pinStates[sizeof(VALID_PINS) / sizeof(VALID_PINS[0])];
unsigned long lastStatusPublish = 0;

int findPinIndex(int pin) {
  for (int i = 0; i < VALID_PINS_COUNT; i++)
    if (VALID_PINS[i] == pin) return i;
  return -1;
}

void updatePinState(int pin, int value, ControlSource source = SOURCE_NONE) {
  int idx = findPinIndex(pin);
  if (idx == -1) return;
  pinStates[idx].pin    = pin;
  pinStates[idx].value  = value;
  pinStates[idx].active = true;
  pinStates[idx].source = source;
}

ControlSource getPinSource(int pin) {
  int idx = findPinIndex(pin);
  if (idx == -1) return SOURCE_NONE;
  return pinStates[idx].source;
}

bool isValidPin(int pin) { return findPinIndex(pin) != -1; }

// ── Set relay ─────────────────────────────────────────────────
void setRelay(int relayPin, int value, ControlSource source) {
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, value);
  updatePinState(relayPin, value, source);
  Serial.printf("[RELAY] GPIO%d = %d (source=%d)\n", relayPin, value, source);
}

// ── Build JSON / Publish ──────────────────────────────────────
String buildStatusJson() {
  int activeCount = 0;
  for (int i = 0; i < VALID_PINS_COUNT; i++)
    if (pinStates[i].active) activeCount++;

  DynamicJsonDocument doc(64 + activeCount * 32);
  doc["status"] = "ok";
  JsonArray pins = doc.createNestedArray("pins");
  for (int i = 0; i < VALID_PINS_COUNT; i++) {
    if (!pinStates[i].active) continue;
    JsonObject obj = pins.createNestedObject();
    obj["pin"]   = pinStates[i].pin;
    obj["value"] = pinStates[i].value;
  }
  String output;
  serializeJson(doc, output);
  return output;
}

void publishPinStatus() {
  if (!mqttClient.connected()) return;
  String payload = buildStatusJson();
  bool ok = mqttClient.publish(MQTT_TOPIC_STATUS.c_str(), payload.c_str(), true);
  Serial.println("[MQTT] Status -> " + MQTT_TOPIC_STATUS);
  Serial.println("[MQTT] Payload: " + payload);
  Serial.println(ok ? "[MQTT] Published OK" : "[MQTT] Publish FAILED");
}

// ── controlPin (MQTT / HTTP) ──────────────────────────────────
String controlPin(String jsonBody, ControlSource source) {
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, jsonBody))
    return "{\"status\":\"error\",\"message\":\"Invalid JSON\"}";

  if (!doc.containsKey("pin") || !doc.containsKey("value"))
    return "{\"status\":\"error\",\"message\":\"Missing 'pin' or 'value'\"}";

  int pin   = doc["pin"].as<int>();
  int value = doc["value"].as<int>();

  if (!isValidPin(pin))
    return "{\"status\":\"error\",\"message\":\"Invalid pin number\"}";
  if (value != 0 && value != 1)
    return "{\"status\":\"error\",\"message\":\"Value must be 0 or 1\"}";

  int pairedRF    = rfPinFromRelay(pin);
  int pairedRelay = relayPinFromRF(pin);

  if (pairedRF != -1) {
    // Lệnh nhắm vào relay pin
    setRelay(pin, value, source);
    updatePinState(pairedRF, digitalRead(pairedRF), source);
  } else if (pairedRelay != -1) {
    // Lệnh nhắm vào RF pin → điều khiển relay tương ứng
    setRelay(pairedRelay, value, source);
    updatePinState(pin, digitalRead(pin), source);
  } else {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, value);
    updatePinState(pin, value, source);
    Serial.printf("[PIN] GPIO%d = %d\n", pin, value);
  }

  publishPinStatus();
  return "{\"status\":\"ok\",\"pin\":" + String(pin) +
         ",\"value\":" + String(value) + "}";
}

// ── syncRFToRelay ─────────────────────────────────────────────
// Chỉ sync khi relay đang do RF điều khiển (SOURCE_RF hoặc SOURCE_NONE)
// Nếu relay đang do MQTT/HTTP điều khiển → bỏ qua tín hiệu RF
// Trừ khi RF thay đổi so với lần đọc trước → người dùng chủ động nhấn remote
//   → RF giành lại quyền điều khiển
void syncRFToRelay() {
  for (int i = 0; i < RF_RELAY_COUNT; i++) {
    int rfPin    = RF_RELAY_MAP[i].rfPin;
    int relayPin = RF_RELAY_MAP[i].relayPin;

    int rfValue    = digitalRead(rfPin);
    int relayValue = digitalRead(relayPin);

    ControlSource relaySource = getPinSource(relayPin);

    // Lấy giá trị RF lần trước từ state
    int idx = findPinIndex(rfPin);
    int prevRfValue = (idx != -1 && pinStates[idx].active)
                      ? pinStates[idx].value
                      : rfValue; // nếu chưa có state thì coi như không đổi

    bool rfChanged = (rfValue != prevRfValue); // người dùng vừa nhấn remote

    if (relaySource == SOURCE_MQTT || relaySource == SOURCE_HTTP) {
      if (!rfChanged) {
        // RF không thay đổi, relay đang do MQTT/HTTP giữ → bỏ qua
        continue;
      }
      // RF thay đổi → người dùng chủ động nhấn remote → RF giành quyền điều khiển
      Serial.printf("[RF OVERRIDE] RF GPIO%d: %d→%d, giành quyền từ MQTT/HTTP\n",
                    rfPin, prevRfValue, rfValue);
    }

    // Relay chưa khớp với RF → cập nhật
    if (rfValue != relayValue || rfChanged) {
      Serial.printf("[RF SYNC] RF GPIO%d=%d → Relay GPIO%d=%d\n",
                    rfPin, rfValue, relayPin, rfValue);
      setRelay(relayPin, rfValue, SOURCE_RF);
      updatePinState(rfPin, rfValue, SOURCE_RF);
      publishPinStatus();
    }
  }
}

// ── REST API ──────────────────────────────────────────────────
void handlePostPin() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json",
      "{\"status\":\"error\",\"message\":\"Empty body\"}"); return;
  }
  String result = controlPin(server.arg("plain"), SOURCE_HTTP);
  server.send(result.indexOf("\"error\"") != -1 ? 400 : 200,
              "application/json", result);
}

void handleGetStatus() {
  String json = "{\"device\":\"ESP8266\",\"ip\":\"";
  json += WiFi.localIP().toString();
  json += "\",\"mqtt_connected\":";
  json += mqttClient.connected() ? "true" : "false";
  json += ",\"pin_status\":" + buildStatusJson() + "}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "application/json",
    "{\"status\":\"error\",\"message\":\"Endpoint not found\"}");
}

// ── MQTT ──────────────────────────────────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
  Serial.println("[MQTT] Topic: " + String(topic));
  Serial.println("[MQTT] Message: " + message);
  String result = controlPin(message, SOURCE_MQTT);
  Serial.println("[MQTT] Result: " + result);
  mqttClient.publish((String(topic) + "/response").c_str(), result.c_str());
}

void reconnectMQTT() {
  if (mqttClient.connected()) return;
  Serial.print("[MQTT] Connecting...");
  bool connected = (strlen(MQTT_USER) > 0)
    ? mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)
    : mqttClient.connect(MQTT_CLIENT_ID);
  if (connected) {
    Serial.println(" Connected!");
    mqttClient.subscribe(MQTT_TOPIC_SUB.c_str());
    Serial.println("[MQTT] Subscribed: " + MQTT_TOPIC_SUB);
    publishPinStatus();
  } else {
    Serial.printf(" Failed (rc=%d), retry in 5s\n", mqttClient.state());
  }
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[BOOT] ESP8266 PIN Controller");

  MQTT_TOPIC_SUB    = "esp8266/" + String(MQTT_CLIENT_ID) + "/pin";
  MQTT_TOPIC_STATUS = "esp8266/" + String(MQTT_CLIENT_ID) + "/status_pin";

  for (int i = 0; i < VALID_PINS_COUNT; i++)
    pinStates[i] = {VALID_PINS[i], 0, false, SOURCE_NONE};

  for (int i = 0; i < RF_RELAY_COUNT; i++) {
    pinMode(RF_RELAY_MAP[i].rfPin, INPUT);
    pinMode(RF_RELAY_MAP[i].relayPin, OUTPUT);
    digitalWrite(RF_RELAY_MAP[i].relayPin, LOW);
    updatePinState(RF_RELAY_MAP[i].relayPin, 0, SOURCE_NONE);
    // Đọc trạng thái ban đầu của RF pin
    updatePinState(RF_RELAY_MAP[i].rfPin,
                   digitalRead(RF_RELAY_MAP[i].rfPin), SOURCE_NONE);
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n[WiFi] IP: " + WiFi.localIP().toString());

  server.on("/pin", HTTP_POST, handlePostPin);
  server.on("/status", HTTP_GET, handleGetStatus);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("[HTTP] Server started on port 80");

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  reconnectMQTT();

  lastStatusPublish = millis();
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  if (!mqttClient.connected()) {
    static unsigned long lastRetry = 0;
    if (millis() - lastRetry > 5000) {
      lastRetry = millis();
      reconnectMQTT();
    }
  }
  mqttClient.loop();

  syncRFToRelay();

  if (millis() - lastStatusPublish >= STATUS_INTERVAL_MS) {
    lastStatusPublish = millis();
    Serial.println("[TIMER] Publishing periodic pin status...");
    publishPinStatus();
  }
}
```

## Tóm tắt logic mới
```bash
Relay source = SOURCE_RF / SOURCE_NONE:
  → syncRFToRelay() hoạt động bình thường

Relay source = SOURCE_MQTT / SOURCE_HTTP:
  RF không thay đổi  → bỏ qua, giữ nguyên giá trị MQTT/HTTP
  RF thay đổi        → người dùng nhấn remote → RF giành quyền điều khiển
```

**Kịch bản lỗi cũ đã được fix:**
```bash
Remote RF tắt  → GPIO16=0 → Relay GPIO14=0 (SOURCE_RF)   ✓
MQTT bật relay → GPIO14=1 (SOURCE_MQTT)                   ✓
syncRFToRelay():
  rfValue=0, prevRfValue=0 → rfChanged=false
  relaySource=SOURCE_MQTT  → bỏ qua ✓  (không ghi đè nữa)
Remote RF bật  → GPIO16=1 → rfChanged=true → RF giành quyền → Relay=1 ✓
Remote RF tắt  → GPIO16=0 → rfChanged=true → RF giành quyền → Relay=0 ✓
```
