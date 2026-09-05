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

// Mã định danh của thiết bị (duy nhất)
// Cần thay đổi khi biên dịch cho thiết bị khác
const char* MQTT_CLIENT_ID = "TAM_10";

// Tên thiết bị --> đồng bộ với backend
// xem file app/services/mqtt_service.rb
// cap nhat gia tri TOPIC_PREFIX trong file mqtt_service.rb khi thay đổi
String CLIENT_NAME = "doraremote/v01/esp8266";

const unsigned long STATUS_INTERVAL_MS = 60000;
const unsigned long WIFI_RETRY_MS      = 10000; // thử kết nối lại WiFi mỗi 10s
const unsigned long WIFI_TIMEOUT_MS    = 15000; // timeout 1 lần thử WiFi
// =================================================

String MQTT_TOPIC_SUB;
String MQTT_TOPIC_STATUS;

ESP8266WebServer server(80);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ── Trạng thái WiFi/HTTP server ───────────────────────────────
bool wifiConnected   = false;
bool httpServerStarted = false;

unsigned long wifiRetryTimer    = 0; // thời điểm thử lại
unsigned long wifiConnectStart  = 0; // thời điểm bắt đầu thử lần này
bool          wifiConnecting    = false;

// ── Nguồn điều khiển ─────────────────────────────────────────
enum ControlSource { SOURCE_NONE, SOURCE_RF, SOURCE_MQTT, SOURCE_HTTP };

// ── Mapping RF ↔ Relay ────────────────────────────────────────
struct RFRelayMap { int rfPin; int relayPin; };

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
  ControlSource source;
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
  return (idx == -1) ? SOURCE_NONE : pinStates[idx].source;
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
  if (!mqttClient.connected()) return; // silent skip nếu không có MQTT
  String payload = buildStatusJson();
  bool ok = mqttClient.publish(MQTT_TOPIC_STATUS.c_str(), payload.c_str(), true);
  Serial.println("[MQTT] Status -> " + MQTT_TOPIC_STATUS);
  Serial.println("[MQTT] Payload: " + payload);
  Serial.println(ok ? "[MQTT] Published OK" : "[MQTT] Publish FAILED");
}

// ── controlPin ────────────────────────────────────────────────
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
    setRelay(pin, value, source);
    updatePinState(pairedRF, digitalRead(pairedRF), source);
  } else if (pairedRelay != -1) {
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

// ── syncRFToRelay (không phụ thuộc WiFi) ─────────────────────
void syncRFToRelay() {
  for (int i = 0; i < RF_RELAY_COUNT; i++) {
    int rfPin    = RF_RELAY_MAP[i].rfPin;
    int relayPin = RF_RELAY_MAP[i].relayPin;

    int rfValue    = digitalRead(rfPin);
    int relayValue = digitalRead(relayPin);

    ControlSource relaySource = getPinSource(relayPin);

    int idx         = findPinIndex(rfPin);
    int prevRfValue = (idx != -1 && pinStates[idx].active)
                      ? pinStates[idx].value : rfValue;

    bool rfChanged = (rfValue != prevRfValue);

    if (relaySource == SOURCE_MQTT || relaySource == SOURCE_HTTP) {
      if (!rfChanged) continue;
      Serial.printf("[RF OVERRIDE] RF GPIO%d: %d→%d, giành quyền từ MQTT/HTTP\n",
                    rfPin, prevRfValue, rfValue);
    }

    if (rfValue != relayValue || rfChanged) {
      Serial.printf("[RF SYNC] RF GPIO%d=%d → Relay GPIO%d=%d\n",
                    rfPin, rfValue, relayPin, rfValue);
      setRelay(relayPin, rfValue, SOURCE_RF);
      updatePinState(rfPin, rfValue, SOURCE_RF);
      publishPinStatus(); // chỉ gửi nếu MQTT đang kết nối
    }
  }
}

// ── WiFi: bắt đầu 1 lần thử kết nối ─────────────────────────
void startWifiConnect() {
  Serial.println("[WiFi] Đang kết nối tới: " + String(WIFI_SSID));
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiConnecting   = true;
  wifiConnectStart = millis();
}

// ── WiFi: quản lý kết nối non-blocking ───────────────────────
void handleWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      // Vừa kết nối thành công
      wifiConnected  = true;
      wifiConnecting = false;
      Serial.println("\n[WiFi] Đã kết nối! IP: " + WiFi.localIP().toString());

      // Khởi động HTTP server lần đầu
      if (!httpServerStarted) {
        server.on("/pin", HTTP_POST, handlePostPin);
        server.on("/status", HTTP_GET, handleGetStatus);
        server.onNotFound(handleNotFound);
        server.begin();
        httpServerStarted = true;
        Serial.println("[HTTP] Server started on port 80");
      }
    }
    return;
  }

  // WiFi mất kết nối
  if (wifiConnected) {
    wifiConnected = false;
    Serial.println("[WiFi] Mất kết nối!");
  }

  // Đang trong lần thử hiện tại → kiểm tra timeout
  if (wifiConnecting) {
    if (millis() - wifiConnectStart < WIFI_TIMEOUT_MS) return; // chờ tiếp
    // Timeout lần này
    wifiConnecting = false;
    wifiRetryTimer = millis();
    Serial.println("[WiFi] Timeout, thử lại sau " +
                   String(WIFI_RETRY_MS / 1000) + "s");
    return;
  }

  // Chờ đủ thời gian trước khi thử lại
  if (millis() - wifiRetryTimer >= WIFI_RETRY_MS) {
    startWifiConnect();
  }
}

// ── MQTT: kết nối lại non-blocking ───────────────────────────
void handleMqtt() {
  if (!wifiConnected) return; // không có WiFi → bỏ qua

  if (mqttClient.connected()) {
    mqttClient.loop();
    return;
  }

  static unsigned long lastMqttRetry = 0;
  if (millis() - lastMqttRetry < 5000) return;
  lastMqttRetry = millis();

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
    Serial.printf(" Failed (rc=%d)\n", mqttClient.state());
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
  json += "\",\"wifi_connected\":";
  json += wifiConnected ? "true" : "false";
  json += ",\"mqtt_connected\":";
  json += mqttClient.connected() ? "true" : "false";
  json += ",\"pin_status\":" + buildStatusJson() + "}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "application/json",
    "{\"status\":\"error\",\"message\":\"Endpoint not found\"}");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
  Serial.println("[MQTT] Topic: " + String(topic));
  Serial.println("[MQTT] Message: " + message);
  String result = controlPin(message, SOURCE_MQTT);
  Serial.println("[MQTT] Result: " + result);
  mqttClient.publish((String(topic) + "/response").c_str(), result.c_str());
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[BOOT] ESP8266 PIN Controller");

  // MQTT_TOPIC_SUB    = "esp8266/" + String(MQTT_CLIENT_ID) + "/pin";
  // MQTT_TOPIC_STATUS = "esp8266/" + String(MQTT_CLIENT_ID) + "/status_pin";
  // Build topic từ CLIENT_ID
  MQTT_TOPIC_SUB = CLIENT_NAME + "/" + String(MQTT_CLIENT_ID) + "/pin";
  MQTT_TOPIC_STATUS = CLIENT_NAME + "/" + String(MQTT_CLIENT_ID) + "/status_pin";

  Serial.println("[MQTT] Sub topic:    " + MQTT_TOPIC_SUB);
  Serial.println("[MQTT] Status topic: " + MQTT_TOPIC_STATUS);

  // Khởi tạo pinStates
  for (int i = 0; i < VALID_PINS_COUNT; i++)
    pinStates[i] = {VALID_PINS[i], 0, false, SOURCE_NONE};

  // ✅ RF + Relay khởi tạo NGAY — không cần WiFi
  for (int i = 0; i < RF_RELAY_COUNT; i++) {
    pinMode(RF_RELAY_MAP[i].rfPin, INPUT);
    pinMode(RF_RELAY_MAP[i].relayPin, OUTPUT);
    digitalWrite(RF_RELAY_MAP[i].relayPin, LOW);
    updatePinState(RF_RELAY_MAP[i].relayPin, 0, SOURCE_NONE);
    updatePinState(RF_RELAY_MAP[i].rfPin,
                   digitalRead(RF_RELAY_MAP[i].rfPin), SOURCE_NONE);
  }
  Serial.println("[RF] RF + Relay sẵn sàng");

  // MQTT callback
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  // Bắt đầu thử kết nối WiFi (non-blocking)
  startWifiConnect();

  lastStatusPublish = millis();
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
  // ✅ RF luôn chạy đầu tiên, không phụ thuộc WiFi
  syncRFToRelay();

  // WiFi & MQTT non-blocking
  handleWifi();
  handleMqtt();

  // HTTP server chỉ chạy khi đã có WiFi
  if (httpServerStarted) server.handleClient();

  // Publish status định kỳ
  if (millis() - lastStatusPublish >= STATUS_INTERVAL_MS) {
    lastStatusPublish = millis();
    Serial.println("[TIMER] Publishing periodic pin status...");
    publishPinStatus();
  }
}
