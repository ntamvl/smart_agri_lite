#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <TZ.h>
#include <FS.h>
#include <LittleFS.h>
#include <CertStoreBearSSL.h>

String CLIENT_VERSION = "0.2.1";

// Set keep-alive 180 seconds
#define BROKER_KEEPALIVE 5 * 60

// ========== CẤU HÌNH - SỬA TẠI ĐÂY ==========
const char* WIFI_SSID     = "[Wifi SSID]";
const char* WIFI_PASSWORD = "[Wifi Password]";

// Cấu hình MQTT
// Đăng ký tài khoản HiveMQ (miễn phí) https://console.hivemq.cloud/
const char* MQTT_SERVER    = "[MQTT Broker URL]"; // ví dụ: "mqtt.hivemq.com" hoặc "broker.emqx.io"
const int   MQTT_PORT      = 8883;
const char* MQTT_USER      = "[MQTT Username]"; // để trống nếu broker không yêu cầu auth
const char* MQTT_PASSWORD  = "[MQTT Password]";

// Mã định danh của thiết bị (duy nhất)
// Cần thay đổi khi biên dịch cho thiết bị khác
const char* MQTT_CLIENT_ID = "TAM_VINHLONG_01_ESP8266_PIN";

// Tên thiết bị --> đồng bộ với backend
String CLIENT_NAME = "vinhlong/tamvinhlong/01/esp8266";

// MQTT CLient Test
// subscribe topic: vinhlong/tamvinhlong/01/esp8266/TAM_VINHLONG_01_ESP8266_PIN/status_pin
// mosquitto_sub -h <host> -u <username> -P <password> -p <PORT> -t vinhlong/tamvinhlong/01/esp8266/TAM_VINHLONG_01_ESP8266_PIN/status_pin
// send command to control pin: vinhlong/tamvinhlong/01/esp8266/TAM_VINHLONG_01_ESP8266_PIN/pin -m '{"pin": 2, "value": 0}'
// mosquitto_pub -h <host> -u <username> -P <password> -p <PORT> -t vinhlong/tamvinhlong/01/esp8266/TAM_VINHLONG_01_ESP8266_PIN/pin -m '{"pin": 2, "value": 0}'

// Topic được build tự động từ MQTT_CLIENT_ID trong setup()
// MQTT_TOPIC_SUB --> {CLIENT_NAME}/{MQTT_CLIENT_ID}/pin
// MQTT_TOPIC_STATUS --> {CLIENT_NAME}/{MQTT_CLIENT_ID}/status_pin
String MQTT_TOPIC_SUB;    // vinhlong/tamvinhlong/01/esp8266/TAM_VINHLONG_01_ESP8266_PIN/pin
String MQTT_TOPIC_STATUS; // vinhlong/tamvinhlong/01/esp8266/TAM_VINHLONG_01_ESP8266_PIN/status_pin

// Lưu IP Address
String ipAddress;

const unsigned long STATUS_INTERVAL_MS = 2000; // 2 giây
// =================================================

ESP8266WebServer server(80);

// WiFiClient wifiClient;
BearSSL::WiFiClientSecure wifiClient;

PubSubClient mqttClient(wifiClient);

BearSSL::CertStore certStore;

// ── Danh sách PIN theo dõi trạng thái ─────────────────────────
const int VALID_PINS[]    = {0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16};
const int VALID_PINS_COUNT = sizeof(VALID_PINS) / sizeof(VALID_PINS[0]);

// Lưu trạng thái các pin đã từng được điều khiển
struct PinState {
  int  pin;
  int  value;
  bool active; // đã được set chưa
};

PinState pinStates[sizeof(VALID_PINS) / sizeof(VALID_PINS[0])];

unsigned long lastStatusPublish = 0;

// ── Tìm index của pin trong mảng pinStates ────────────────────
int findPinIndex(int pin) {
  for (int i = 0; i < VALID_PINS_COUNT; i++) {
    if (VALID_PINS[i] == pin) return i;
  }
  return -1;
}

// ── Cập nhật trạng thái pin vào mảng ─────────────────────────
void updatePinState(int pin, int value) {
  int idx = findPinIndex(pin);
  if (idx == -1) return;
  pinStates[idx].pin    = pin;
  pinStates[idx].value  = value;
  pinStates[idx].active = true;
}

// ── Build JSON trạng thái toàn bộ pin đang active ────────────
String buildStatusJson() {
  // Đếm số pin active
  int activeCount = 0;
  for (int i = 0; i < VALID_PINS_COUNT; i++) {
    if (pinStates[i].active) activeCount++;
  }

  // Kích thước: 64 bytes overhead + 32 bytes/pin
  DynamicJsonDocument doc(64 + activeCount * 32);

  doc["status"] = "ok";
  doc["ip"] = ipAddress;
  doc["version"] = CLIENT_VERSION;

  // Memory overflow error occurs when the pin array has too many elements.
  // JsonArray pins = doc.createNestedArray("pins");

  // for (int i = 0; i < VALID_PINS_COUNT; i++) {
  //   if (!pinStates[i].active) continue;
  //   JsonObject obj = pins.createNestedObject();
  //   obj["pin"]   = pinStates[i].pin;
  //   obj["value"] = pinStates[i].value;
  // }

  String output;
  serializeJson(doc, output);
  return output;
}

// ── Publish trạng thái pin lên MQTT ──────────────────────────
void publishPinStatus() {
  // if (!mqttClient.connected()) return;

  String topic = MQTT_TOPIC_STATUS;
  String payload = buildStatusJson();

  bool ok = mqttClient.publish(topic.c_str(), payload.c_str(), true); // retain=true
  Serial.println("[MQTT] Status publish -> " + topic);
  Serial.println("[MQTT] Payload: " + payload);
  Serial.println(ok ? "[MQTT] Published OK" : "[MQTT] Publish FAILED");
}

// ── Kiểm tra PIN hợp lệ ───────────────────────────────────────
bool isValidPin(int pin) {
  return findPinIndex(pin) != -1;
}

// ── Xử lý JSON và điều khiển PIN ──────────────────────────────
String controlPin(String jsonBody) {
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, jsonBody);

  if (error)
    return "{\"status\":\"error\",\"message\":\"Invalid JSON\"}";

  if (!doc.containsKey("pin") || !doc.containsKey("value"))
    return "{\"status\":\"error\",\"message\":\"Missing 'pin' or 'value'\"}";

  int pin   = doc["pin"].as<int>();
  int value = doc["value"].as<int>();

  if (!isValidPin(pin))
    return "{\"status\":\"error\",\"message\":\"Invalid pin number\"}";

  if (value != 0 && value != 1)
    return "{\"status\":\"error\",\"message\":\"Value must be 0 or 1\"}";

  pinMode(pin, OUTPUT);
  digitalWrite(pin, value);
  updatePinState(pin, value);   // ← lưu trạng thái

  Serial.printf("[PIN] GPIO%d = %d\n", pin, value);

  // Publish status ngay sau khi có thay đổi
  publishPinStatus();

  return "{\"status\":\"ok\",\"pin\":" + String(pin) +
         ",\"value\":" + String(value) + "}";
}

// ── REST API Handlers ──────────────────────────────────────────
void handlePostPin() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json",
      "{\"status\":\"error\",\"message\":\"Empty body\"}");
    return;
  }
  String body = server.arg("plain");
  Serial.println("[HTTP] POST /pin -> " + body);
  String result = controlPin(body);
  int code = result.indexOf("\"error\"") != -1 ? 400 : 200;
  server.send(code, "application/json", result);
}

void handleGetStatus() {
  String json = "{\"device\":\"ESP8266\",\"ip\":\"";
  json += WiFi.localIP().toString();
  json += "\",\"mqtt_connected\":";
  json += mqttClient.connected() ? "true" : "false";
  json += ",\"pin_status\":" + buildStatusJson();
  json += "}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "application/json",
    "{\"status\":\"error\",\"message\":\"Endpoint not found\"}");
}

// ── MQTT Callback ──────────────────────────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];

  Serial.println("[MQTT] Topic: " + String(topic));
  Serial.println("[MQTT] Message: " + message);

  String result = controlPin(message); // publishPinStatus() gọi bên trong
  Serial.println("[MQTT] Result: " + result);

  String responseTopic = String(topic) + "/response";
  mqttClient.publish(responseTopic.c_str(), result.c_str());
}

// ── Kết nối lại MQTT ──────────────────────────────────────────
void reconnectMQTT() {
  // Set keep-alive to 180 seconds
  // mqttClient.setKeepAlive(BROKER_KEEPALIVE);

  if (mqttClient.connected()) return;

  Serial.print("[MQTT] Connecting...");
  bool connected = (strlen(MQTT_USER) > 0)
    ? mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)
    : mqttClient.connect(MQTT_CLIENT_ID);

  if (connected) {
    Serial.println(" Connected!");
    mqttClient.subscribe(MQTT_TOPIC_SUB.c_str());
    Serial.println("[MQTT] Subscribed: " + String(MQTT_TOPIC_SUB));
    publishPinStatus(); // gửi status ngay khi kết nối lại
  } else {
    Serial.printf(" Failed (rc=%d), retry in 5s\n", mqttClient.state());
  }
}

void reconnectMQTTV2() {
  // delay(1000);

  // Set keep-alive to 180 seconds
  // mqttClient.setKeepAlive(BROKER_KEEPALIVE);
  // mqttClient.setSocketTimeout(BROKER_KEEPALIVE);

  // Loop until we’re reconnected
  while (!mqttClient.connected()) {
    Serial.print("[MQTT] Attempting MQTT connection...");

    // Attempt to connect
    // Insert your password
    bool connected = (strlen(MQTT_USER) > 0)
      ? mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)
      : mqttClient.connect(MQTT_CLIENT_ID);

    if (connected) {
      Serial.println(" Connected!");
      Serial.println("[MQTT] state: " + String(mqttClient.state()));

      // Once connected, publish an announcement…
      mqttClient.subscribe(MQTT_TOPIC_SUB.c_str());
      Serial.println("[MQTT] Subscribed: " + String(MQTT_TOPIC_SUB));

      publishPinStatus(); // gửi status ngay khi kết nối lại
    } else {
      Serial.print("failed, rc = ");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void reconnectMQTTV3() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("[MQTT] Attempting MQTT connection...");

    // Create a random client ID
    // String clientId = "ESP8266Client-";
    // clientId += String(random(0xffff), HEX);

    bool connected = (strlen(MQTT_USER) > 0) ? mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD) : mqttClient.connect(MQTT_CLIENT_ID);

    // Attempt to connect
    if (connected) {
      Serial.println(" Connected!");
      Serial.println("[MQTT] state: " + String(mqttClient.state()));

      mqttClient.subscribe(MQTT_TOPIC_SUB.c_str());
      Serial.println("[MQTT] Subscribed: " + String(MQTT_TOPIC_SUB));

      Serial.println("[MQTT] Publish status");
      publishPinStatus(); // gửi status ngay khi kết nối lại
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setupWifi() {
  delay(100);

  // you can use the insecure mode, when you want to avoid the certificates
  wifiClient.setInsecure();

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  ipAddress = WiFi.localIP().toString();
}

void setDateTime() {
  // You can use your own timezone, but the exact time is not used at all.
  // Only the date is needed for validating the certificates.
  configTime(TZ_Asia_Ho_Chi_Minh, "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.printf("%s %s", tzname[0], asctime(&timeinfo));
}

void setupData() {
  // Build topic từ CLIENT_ID
  MQTT_TOPIC_SUB    = CLIENT_NAME + "/" + String(MQTT_CLIENT_ID) + "/pin";
  MQTT_TOPIC_STATUS = CLIENT_NAME + "/" + String(MQTT_CLIENT_ID) + "/status_pin";

  Serial.println("[MQTT] Sub topic:    " + MQTT_TOPIC_SUB);
  Serial.println("[MQTT] Status topic: " + MQTT_TOPIC_STATUS);

  // Khởi tạo mảng pinStates
  for (int i = 0; i < VALID_PINS_COUNT; i++) {
    pinStates[i] = {VALID_PINS[i], 0, false};
  }
}

// ── Setup ──────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[BOOT] ESP8266 PIN Controller");
  Serial.println("CLIENT_VERSION: " + CLIENT_VERSION);

  setupData();
  setupWifi();
  // setDateTime();

  // REST API
  server.on("/pin", HTTP_POST, handlePostPin);
  server.on("/status", HTTP_GET, handleGetStatus);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("[HTTP] Server started on port 80");

  // MQTT
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  lastStatusPublish = millis();
}

// ── Loop ───────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  if (!mqttClient.connected()) {
    reconnectMQTTV3();
  }
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastStatusPublish > STATUS_INTERVAL_MS) {
    lastStatusPublish = now;
    Serial.println("[TIMER2] Publishing periodic pin status...");
    publishPinStatus();
  }
}
