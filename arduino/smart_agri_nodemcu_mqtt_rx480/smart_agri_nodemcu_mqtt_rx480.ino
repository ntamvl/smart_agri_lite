#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <TZ.h>
#include <FS.h>
#include <LittleFS.h>
#include <CertStoreBearSSL.h>

#define ledPin LED_BUILTIN // GPIO2

#define rfPin1 D0  // GPIO16 (16)
#define rfPin2 D1  // GPIO5 (5)
#define rfPin3 D2  // GPIO4 (4)

String CLIENT_VERSION = "0.3.1";

// Set keep-alive 180 seconds
#define BROKER_KEEPALIVE 5 * 60

// ========== CẤU HÌNH - SỬA TẠI ĐÂY ==========
const char* WIFI_SSID = "[WIFI_SSID]";
const char* WIFI_PASSWORD = "[WIFI_PASSWORD]";

// Cấu hình MQTT
// Đăng ký tài khoản HiveMQ (miễn phí) https://console.hivemq.cloud/
const char* MQTT_SERVER = "[MQTT_SERVER]";
const int MQTT_PORT = 8883;
const char* MQTT_USER = "[MQTT_USER]";
const char* MQTT_PASSWORD = "[MQTT_PASSWORD]";

// Mã định danh của thiết bị (duy nhất)
// Cần thay đổi khi biên dịch cho thiết bị khác
const char* MQTT_CLIENT_ID = "TAM_10";

// Tên thiết bị --> đồng bộ với backend
// xem file app/services/mqtt_service.rb
// cap nhat gia tri TOPIC_PREFIX trong file mqtt_service.rb khi thay đổi
String CLIENT_NAME = "doraremote/v01/esp8266";

// MQTT CLient Test
// subscribe topic: doraremote/v01/esp8266/TAM_10/status_pin
// mosquitto_sub -h <host> -u <username> -P <password> -p <PORT> -t doraremote/v01/esp8266/TAM_10/status_pin
// send command to control pin: doraremote/v01/esp8266/TAM_10/pin -m '{"pin": 2, "value": 0}'
// mosquitto_pub -h <host> -u <username> -P <password> -p <PORT> -t doraremote/v01/esp8266/TAM_10/pin -m '{"pin": 2, "value": 0}'

// Topic được build tự động từ MQTT_CLIENT_ID trong setup()
// MQTT_TOPIC_SUB --> {CLIENT_NAME}/{MQTT_CLIENT_ID}/pin
// MQTT_TOPIC_STATUS --> {CLIENT_NAME}/{MQTT_CLIENT_ID}/status_pin
String MQTT_TOPIC_SUB;     // doraremote/v01/esp8266/TAM_10/pin
String MQTT_TOPIC_STATUS;  // doraremote/v01/esp8266/TAM_10/status_pin

// Lưu IP Address
String ipAddress;

const unsigned long STATUS_INTERVAL_MS = 5 * 1000;  // 5 giây
// =================================================

ESP8266WebServer server(80);

WiFiClient wifiClient;

// Uncomment for MQTT SSL, port 8883
// BearSSL::WiFiClientSecure wifiClient;

PubSubClient mqttClient(wifiClient);

BearSSL::CertStore certStore;

// ── Nguồn điều khiển relay ────────────────────────────────────
enum ControlSource {
  SOURCE_NONE,  // chưa có lệnh nào
  SOURCE_RF,    // do Remote RF điều khiển
  SOURCE_MQTT,  // do MQTT điều khiển
  SOURCE_HTTP   // do HTTP điều khiển
};


// begin RF
// ── Mapping RF 433MHz (RX480) ↔ Relay ────────────────────────
// Mỗi cặp: { chân RF input, chân Relay output }
struct RFRelayMap {
  int rfPin;     // RX480 output  → ESP8266 input
  int relayPin;  // Relay control → ESP8266 output
};

const RFRelayMap RF_RELAY_MAP[] = {
  { 16, 14 }, // D0 → D5
  // { 16, 2 },  // D0 → LED BUILTIN GPIO2
  { 5, 12 },  // D1 → D6
  { 4, 13 },  // D2 → D7
  { 0, 15 },  // D3 → D8
};
const int RF_RELAY_COUNT = sizeof(RF_RELAY_MAP) / sizeof(RF_RELAY_MAP[0]);

// Tìm relayPin tương ứng với rfPin (trả về -1 nếu không có)
int relayPinFromRF(int rfPin) {
  for (int i = 0; i < RF_RELAY_COUNT; i++)
    if (RF_RELAY_MAP[i].rfPin == rfPin) return RF_RELAY_MAP[i].relayPin;
  return -1;
}

// Tìm rfPin tương ứng với relayPin (trả về -1 nếu không có)
int rfPinFromRelay(int relayPin) {
  for (int i = 0; i < RF_RELAY_COUNT; i++)
    if (RF_RELAY_MAP[i].relayPin == relayPin) return RF_RELAY_MAP[i].rfPin;
  return -1;
}
// end RF

// ── Danh sách PIN theo dõi trạng thái ─────────────────────────
const int VALID_PINS[] = { 0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16 };
const int VALID_PINS_COUNT = sizeof(VALID_PINS) / sizeof(VALID_PINS[0]);

// ── Danh sách PIN OUTPUT ─────────────────────────
const int VALID_OUT_PINS_COUNT = 4;
const int VALID_OUT_PINS[] = { 12, 13, 14, 15 };
// const int VALID_OUT_PINS_COUNT = sizeof(VALID_OUT_PINS) / sizeof(VALID_OUT_PINS[0]);

// ── Danh sách PIN RF REMOTE ─────────────────────────
const int VALID_RF_PINS[] = { 4, 5, 16 };

// Lưu trạng thái các pin đã từng được điều khiển
struct PinState {
  int pin;
  int value;
  bool active;  // đã được set chưa
  ControlSource source; // ← nguồn điều khiển
};

PinState pinStates[sizeof(VALID_PINS) / sizeof(VALID_PINS[0])];

unsigned long lastStatusPublish = 0;

void setupValidOutPins() {
  // for (int i = 0; i < VALID_PINS_COUNT; i++) {
  //   Serial.println("Setup pin " + String(VALID_OUT_PINS[i]) + " OUTPUT");
  //   pinMode(VALID_OUT_PINS[i], OUTPUT);
  // }

  pinMode(VALID_RF_PINS[0], INPUT_PULLUP);
  pinMode(VALID_RF_PINS[1], INPUT_PULLUP);
  pinMode(VALID_RF_PINS[2], INPUT_PULLUP);

  pinMode(VALID_OUT_PINS[0], OUTPUT);
  pinMode(VALID_OUT_PINS[1], OUTPUT);
  pinMode(VALID_OUT_PINS[2], OUTPUT);
  pinMode(VALID_OUT_PINS[3], OUTPUT);
}

void setupRFRemotePins() {
  // RF 433MHz Học Lệnh RX480
  pinMode(rfPin1, INPUT_PULLUP);
  pinMode(rfPin2, INPUT_PULLUP);
  pinMode(rfPin3, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
}

void printRFLog(int pin) {
  int state = digitalRead(pin);
  String stateValue = state == HIGH ? "HIGH" : "LOW";
  Serial.println("Trang thai chan pin " + String(pin) + " = " + String(state) + " - state: " + stateValue);
}

// ── Tìm index của pin trong mảng pinStates ────────────────────
int findPinIndex(int pin) {
  for (int i = 0; i < VALID_PINS_COUNT; i++) {
    if (VALID_PINS[i] == pin) return i;
  }
  return -1;
}

// ── Cập nhật trạng thái pin vào mảng ─────────────────────────
void updatePinState(int pin, int value, ControlSource source = SOURCE_NONE) {
  int idx = findPinIndex(pin);
  if (idx == -1) return;
  pinStates[idx].pin = pin;
  pinStates[idx].value = value;
  pinStates[idx].active = true;
  pinStates[idx].source = source;
  Serial.println("pin: " + String(pin) + "; value: " + String(value) + "; source: " + String(source));
}

ControlSource getPinSource(int pin) {
  int idx = findPinIndex(pin);
  if (idx == -1) return SOURCE_NONE;
  return pinStates[idx].source;
}

// ── Ghi relay (output) và cập nhật state ─────────────────────
void setRelay(int relayPin, int value, ControlSource source) {
  Serial.printf("[RELAY] SET relay pin %d = %d \n", relayPin, value);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, value);
  updatePinState(relayPin, value, source);
  Serial.printf("[RELAY] GPIO%d = %d (source=%d)\n", relayPin, value, source);
}

// ── Ghi RF pin (output ngược lại lên RX480) và cập nhật state
void setRFPin(int rfPin, int value) {
  pinMode(rfPin, OUTPUT);
  Serial.printf("[RF OUT] Change mode GPIO%d to OUTPUT \n", rfPin);

  digitalWrite(rfPin, value);
  updatePinState(rfPin, value, SOURCE_NONE);
  Serial.printf("[RF OUT] GPIO%d = %d\n", rfPin, value);

  Serial.printf("[RF OUT] Waiting to change mode GPIO%d to INPUT_PULLUP \n", rfPin);
  delay(1000);

  pinMode(rfPin, INPUT_PULLUP);
  Serial.printf("[RF OUT] Change mode GPIO%d to INPUT_PULLUP \n", rfPin);
  delay(1000);
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

  bool ok = mqttClient.publish(topic.c_str(), payload.c_str(), true);  // retain=true
  Serial.println("[MQTT] Status publish -> " + topic);
  Serial.println("[MQTT] Payload: " + payload);
  Serial.println(ok ? "[MQTT] Published OK" : "[MQTT] Publish FAILED");
}

// ── Kiểm tra PIN hợp lệ ───────────────────────────────────────
bool isValidPin(int pin) {
  return findPinIndex(pin) != -1;
}

// ── Xử lý JSON và điều khiển PIN ──────────────────────────────
String controlPin(String jsonBody, ControlSource source) {
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, jsonBody);

  if (error)
    return "{\"status\":\"error\",\"message\":\"Invalid JSON\"}";

  if (!doc.containsKey("pin") || !doc.containsKey("value"))
    return "{\"status\":\"error\",\"message\":\"Missing 'pin' or 'value'\"}";

  int pin = doc["pin"].as<int>();
  int value = doc["value"].as<int>();

  if (!isValidPin(pin))
    return "{\"status\":\"error\",\"message\":\"Invalid pin number\"}";

  if (value != 0 && value != 1)
    return "{\"status\":\"error\",\"message\":\"Value must be 0 or 1\"}";

  // Kiểm tra pin có thuộc cặp RF ↔ Relay không
  int pairedRF = rfPinFromRelay(pin);     // pin là relay → tìm RF tương ứng
  int pairedRelay = relayPinFromRF(pin);  // pin là RF    → tìm relay tương ứng

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

  // Publish status ngay sau khi có thay đổi
  publishPinStatus();

  return "{\"status\":\"ok\",\"pin\":" + String(pin) + ",\"value\":" + String(value) + "}";
}

// ── Đọc RF input và đồng bộ relay ────────────────────────────
// ── syncRFToRelay ─────────────────────────────────────────────
// Chỉ sync khi relay đang do RF điều khiển (SOURCE_RF hoặc SOURCE_NONE)
// Nếu relay đang do MQTT/HTTP điều khiển → bỏ qua tín hiệu RF
// Trừ khi RF thay đổi so với lần đọc trước → người dùng chủ động nhấn remote
//   → RF giành lại quyền điều khiển
void syncRFToRelay() {
  for (int i = 0; i < RF_RELAY_COUNT; i++) {
    int rfPin = RF_RELAY_MAP[i].rfPin;
    int relayPin = RF_RELAY_MAP[i].relayPin;

    int rfValue = digitalRead(rfPin);  // đọc thực tế từ RX480
    int relayValue = digitalRead(relayPin);

    ControlSource relaySource = getPinSource(relayPin);

    // Lấy giá trị RF lần trước từ state
    int idx = findPinIndex(rfPin);
    int prevRfValue = (idx != -1 && pinStates[idx].active) ? pinStates[idx].value : rfValue; // nếu chưa có state thì coi như không đổi

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
      Serial.printf("[RF SYNC] RF GPIO%d=%d → Relay GPIO%d=%d\n", rfPin, rfValue, relayPin, rfValue);
      setRelay(relayPin, rfValue, SOURCE_RF);
      updatePinState(rfPin, rfValue, SOURCE_RF);
      publishPinStatus();
    }

    // Serial.printf("[RF SYNC] RF GPIO%d=%d → Relay GPIO%d=%d\n", rfPin, rfValue, relayPin, rfValue);
    // setRelay(relayPin, rfValue);
    // updatePinState(rfPin, rfValue);
    // updatePinState(relayPin, rfValue);
    // publishPinStatus();
  }
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
  String result = controlPin(body, SOURCE_HTTP);
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

  String result = controlPin(message, SOURCE_MQTT);  // publishPinStatus() gọi bên trong
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
    publishPinStatus();  // gửi status ngay khi kết nối lại
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

      publishPinStatus();  // gửi status ngay khi kết nối lại
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
      publishPinStatus();  // gửi status ngay khi kết nối lại

      // Turn on led builtin
      Serial.println("Turn on LED Builtin");
      digitalWrite(ledPin, 0);
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
  // Uncomment for MQTT SSL, port 8883
  // wifiClient.setInsecure();

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
  MQTT_TOPIC_SUB = CLIENT_NAME + "/" + String(MQTT_CLIENT_ID) + "/pin";
  MQTT_TOPIC_STATUS = CLIENT_NAME + "/" + String(MQTT_CLIENT_ID) + "/status_pin";

  Serial.println("[MQTT] Sub topic:    " + MQTT_TOPIC_SUB);
  Serial.println("[MQTT] Status topic: " + MQTT_TOPIC_STATUS);

  // Khởi tạo mảng pinStates
  for (int i = 0; i < VALID_PINS_COUNT; i++) {
    pinStates[i] = { VALID_PINS[i], 0, false };
  }

  // Cấu hình RF pins là INPUT, INPUT_PULLUP, relay pins là OUTPUT
  for (int i = 0; i < RF_RELAY_COUNT; i++) {
    pinMode(RF_RELAY_MAP[i].rfPin, INPUT);
    pinMode(RF_RELAY_MAP[i].relayPin, OUTPUT);
    digitalWrite(RF_RELAY_MAP[i].relayPin, LOW);
    updatePinState(RF_RELAY_MAP[i].relayPin, 0, SOURCE_NONE);
    // Đọc trạng thái ban đầu của RF pin
    updatePinState(RF_RELAY_MAP[i].rfPin, digitalRead(RF_RELAY_MAP[i].rfPin), SOURCE_NONE);
  }
}

void setupLedBuiltin() {
  pinMode(ledPin, OUTPUT);
  Serial.println("[INIT] Turn off LED Builtin");
  digitalWrite(ledPin, 1);
}

// ── Setup ──────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[BOOT] ESP8266 PIN Controller");
  Serial.println("CLIENT_VERSION: " + CLIENT_VERSION);

  // setup RF Remote RX480
  // setupRFRemotePins();
  // setupValidOutPins();

  setupData();
  setupWifi();
  // setDateTime();

  setupLedBuiltin();

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
  // Đọc tín hiệu RF và đồng bộ relay
  syncRFToRelay();

  server.handleClient();

  if (!mqttClient.connected()) {
    reconnectMQTTV3();
  }
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastStatusPublish > STATUS_INTERVAL_MS) {
    lastStatusPublish = now;
    Serial.println("[TIMER] Publishing periodic pin status...");
    publishPinStatus();
  }  
}
