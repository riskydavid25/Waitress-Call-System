#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <time.h>

// ==== KONFIGURASI MQTT ====
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;

// ID Unik Perangkat
const char* deviceID = "Sender1";
const char* mqtt_client_id = "ESP32_Sender1";

// ==== QoS MQTT ====
#define MQTT_QOS_LEVEL 1

// ==== LED Indikator ====
const int greenLed = 12;
const int blueLed = 13;
const int wifiLed = 14;

// ==== OBJEK ====
WiFiClient espClient;
PubSubClient client(espClient);
WiFiManager wifiManager;

// ==== Counter ====
int callCount = 0;
int billCount = 0;

// ==== SETUP WIFI ====
void setup_wifi() {
  wifiManager.setTimeout(180);
  if (!wifiManager.autoConnect("Sender_AP")) {
    Serial.println("⛔ Timeout, Restart ESP");
    ESP.restart();
  }
  Serial.println("✅ WiFi Connected: " + WiFi.localIP().toString());
}

// ==== MQTT RECONNECT ====
void reconnect() {
  while (!client.connected()) {
    Serial.print("🔄 Connecting to MQTT...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("✅ Connected");
    } else {
      Serial.print("⛔ Failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

// ==== KIRIM DATA KE MQTT ====
void sendMessage(const char* topic, const char* type, bool status, int count, int rssi) {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  // Konversi ke zona waktu WIB (UTC+7)
  timeinfo.tm_hour += 7;
  mktime(&timeinfo);

  char isoTimestamp[30];
  strftime(isoTimestamp, sizeof(isoTimestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

  String payload = "{";
  payload += "\"id\":\"" + String(deviceID) + "\",";
  payload += "\"type\":\"" + String(type) + "\",";
  payload += "\"status\":" + String(status ? "true" : "false") + ",";
  payload += "\"count\":" + String(count) + ",";
  payload += "\"rssi\":" + String(rssi) + ",";
  payload += "\"timestamp\":\"" + String(isoTimestamp) + "\"";
  payload += "}";

  client.publish(topic, payload.c_str(), MQTT_QOS_LEVEL, true);
  Serial.printf("📤 Sent to %s: %s\n", topic, payload.c_str());
}

// ==== TASK SIMULASI KIRIMAN ====
void senderTask(void* parameter) {
  while (true) {
    if (!client.connected()) reconnect();
    client.loop();

    int action = random(0, 2); // 0 = call, 1 = bill
    int rssi = WiFi.RSSI();
    String topicBase = "waitress/" + String(deviceID) + "/";

    if (action == 0) {
      callCount++;
      digitalWrite(greenLed, HIGH);
      digitalWrite(blueLed, LOW);
      sendMessage((topicBase + "call").c_str(), "call", true, callCount, rssi);
      sendMessage((topicBase + "bill").c_str(), "bill", false, callCount, rssi);
    } else {
      billCount++;
      digitalWrite(greenLed, LOW);
      digitalWrite(blueLed, HIGH);
      sendMessage((topicBase + "bill").c_str(), "bill", true, billCount, rssi);
      sendMessage((topicBase + "call").c_str(), "call", false, billCount, rssi);
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

// ==== TASK KEDIP LED ====
void wifiLedTask(void* parameter) {
  while (true) {
    digitalWrite(wifiLed, !digitalRead(wifiLed));
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// ==== SETUP ====
void setup() {
  Serial.begin(115200);
  pinMode(greenLed, OUTPUT);
  pinMode(blueLed, OUTPUT);
  pinMode(wifiLed, OUTPUT);
  digitalWrite(greenLed, LOW);
  digitalWrite(blueLed, LOW);
  digitalWrite(wifiLed, HIGH);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  // Setup NTP ke Asia/Jakarta
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  while (time(nullptr) < 100000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n🕐 Waktu sinkron!");

  xTaskCreatePinnedToCore(senderTask, "Sender Task", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(wifiLedTask, "WiFi LED Task", 2048, NULL, 1, NULL, 1);
}

// ==== LOOP ====
void loop() {
  client.loop(); // Jaga koneksi MQTT tetap hidup
}
