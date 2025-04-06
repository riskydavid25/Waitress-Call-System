#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <BlynkSimpleEsp32.h>
#include <time.h>

// MQTT Broker
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_client_id = "ESP32_Sender1";

// Blynk Auth
#define BLYNK_AUTH "cjqSPyXSlzm32sMLG0JOfH7ANlSvqe8M"

// LED Pin
const int greenLed = 12;
const int blueLed = 13;
const int wifiLed = 14;

WiFiClient espClient;
PubSubClient client(espClient);
WiFiManager wifiManager;

// Counter
int callCount = 0;
int billCount = 0;

#define MQTT_QOS_LEVEL 1 // Bisa diganti ke 2 untuk uji QoS 2

void setup_wifi() {
  wifiManager.setTimeout(180);
  if (!wifiManager.autoConnect("Sender1_AP")) {
    Serial.println("Failed to connect and hit timeout");
    ESP.restart();
  }
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void sendMessage(const char* topic, const char* type, bool status, int count, int rssi) {
  time_t now = time(nullptr);
  struct tm* timeinfo = gmtime(&now);

  char isoTimestamp[25];
  strftime(isoTimestamp, sizeof(isoTimestamp), "%Y-%m-%dT%H:%M:%SZ", timeinfo);

  String payload = "{";
  payload += "\"id\":\"Sender1\",";
  payload += "\"type\":\"" + String(type) + "\",";
  payload += "\"status\":" + String(status ? "true" : "false") + ",";
  payload += "\"count\":" + String(count) + ",";
  payload += "\"rssi\":" + String(rssi) + ",";  // Menambahkan RSSI
  payload += "\"timestamp\":\"" + String(isoTimestamp) + "\"";
  payload += "}";

  client.publish(topic, payload.c_str(), MQTT_QOS_LEVEL, true);
  Serial.printf("Published to %s: %s (QoS %d)\n", topic, payload.c_str(), MQTT_QOS_LEVEL);
}

void senderTask(void* parameter) {
  while (true) {
    if (!client.connected()) reconnect();
    client.loop();

    int action = random(0, 2); // 0 = call, 1 = bill

    int rssi = WiFi.RSSI();  // Membaca nilai RSSI

    if (action == 0) {
      callCount++;
      digitalWrite(greenLed, HIGH);
      digitalWrite(blueLed, LOW);
      sendMessage("waitress/sender1/call", "call", true, callCount, rssi);  // Mengirimkan RSSI
      sendMessage("waitress/sender1/bill", "bill", false, callCount, rssi);  // Mengirimkan RSSI
    } else {
      billCount++;
      digitalWrite(greenLed, LOW);
      digitalWrite(blueLed, HIGH);
      sendMessage("waitress/sender1/bill", "bill", true, billCount, rssi);  // Mengirimkan RSSI
      sendMessage("waitress/sender1/call", "call", false, billCount, rssi);  // Mengirimkan RSSI
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS); // Delay 5 detik antar pesan
  }
}

void wifiLedTask(void* parameter) {
  while (true) {
    digitalWrite(wifiLed, !digitalRead(wifiLed));
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

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
  Blynk.begin(BLYNK_AUTH, WiFi.SSID().c_str(), WiFi.psk().c_str());

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  while (time(nullptr) < 100000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWaktu sinkron!");

  xTaskCreatePinnedToCore(senderTask, "Sender Task", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(wifiLedTask, "WiFi LED Task", 2048, NULL, 1, NULL, 1);
}

void loop() {
  Blynk.run();
}
