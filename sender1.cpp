#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <time.h>
#include <BlynkSimpleEsp32.h>

// ==== KONFIGURASI MQTT ====
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* deviceID = "Sender1";
const char* mqtt_client_id = "ESP32_Sender1";

// ==== BLYNK ====
#define BLYNK_PRINT Serial
char auth[] = "cjqSPyXSlzm32sMLG0JOfH7ANlSvqe8M"; // Ganti dengan token kamu

// ==== QoS MQTT ====
#define MQTT_QOS_LEVEL 1

// ==== PIN BUTTON & LED ====
const int callButton = 33;
const int billButton = 25;
const int resetButton = 26;
const int greenLed = 12;
const int blueLed = 13;
const int wifiLed = 14;

// ==== STATUS DAN COUNTER ====
int lastCallState = HIGH;
int lastBillState = HIGH;
int lastResetState = HIGH;
int callCount = 0;
int billCount = 0;
bool firstRun = true;

// ==== OBJEK ====
WiFiClient espClient;
PubSubClient client(espClient);
WiFiManager wifiManager;

TaskHandle_t TaskButton;
TaskHandle_t TaskMQTT;

// ==== SETUP WIFI ====
void setup_wifi() {
  wifiManager.setTimeout(180);
  if (!wifiManager.autoConnect("Sender1_AP")) {
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

// ==== KIRIM DATA KE MQTT (FORMAT JSON) ====
void sendMessage(const char* type, bool status, int count) {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);  // Otomatis Asia/Jakarta

  char formattedTime[40];
  strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%d %H:%M:%S WIB", &timeinfo);

  int rssi = WiFi.RSSI();
  String topic = "waitress/" + String(deviceID) + "/" + String(type);

  String payload = "{";
  payload += "\"id\":\"" + String(deviceID) + "\",";
  payload += "\"type\":\"" + String(type) + "\",";
  payload += "\"status\":" + String(status ? "true" : "false") + ",";
  payload += "\"count\":" + String(count) + ",";
  payload += "\"rssi\":" + String(rssi) + ",";
  payload += "\"timestamp\":\"" + String(formattedTime) + "\"";
  payload += "}";

  client.publish(topic.c_str(), payload.c_str(), MQTT_QOS_LEVEL, true);
  Serial.printf("📤 Sent to %s: %s\n", topic.c_str(), payload.c_str());
}

// ==== RESET SISTEM ====
void resetSystem() {
  digitalWrite(greenLed, LOW);
  digitalWrite(blueLed, LOW);
  sendMessage("call", false, callCount);
  sendMessage("bill", false, billCount);
  Blynk.virtualWrite(V1, 0);
  Blynk.virtualWrite(V2, 0);
  Blynk.virtualWrite(V3, 1);
  Serial.println("🔁 System Reset via Button");
}

// ==== TASK HANDLE BUTTON ====
void TaskHandleButtons(void *pvParameters) {
  for (;;) {
    int callState = digitalRead(callButton);
    int billState = digitalRead(billButton);
    int resetState = digitalRead(resetButton);

    if (callState == LOW && lastCallState == HIGH) {
      callCount++;
      digitalWrite(greenLed, HIGH);
      digitalWrite(blueLed, LOW);
      if (!firstRun) {
        sendMessage("call", true, callCount);
        sendMessage("bill", false, callCount);
      }
      Blynk.virtualWrite(V1, 1);
      Blynk.virtualWrite(V2, 0);
      Blynk.virtualWrite(V3, 0);
      firstRun = false;
      delay(200);
    }

    if (billState == LOW && lastBillState == HIGH) {
      billCount++;
      digitalWrite(greenLed, LOW);
      digitalWrite(blueLed, HIGH);
      if (!firstRun) {
        sendMessage("call", false, billCount);
        sendMessage("bill", true, billCount);
      }
      Blynk.virtualWrite(V1, 0);
      Blynk.virtualWrite(V2, 1);
      Blynk.virtualWrite(V3, 0);
      firstRun = false;
      delay(200);
    }

    if (resetState == LOW && lastResetState == HIGH) {
      resetSystem();
      delay(200);
    }

    lastCallState = callState;
    lastBillState = billState;
    lastResetState = resetState;

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ==== TASK MQTT DAN WIFI LED ====
void TaskMQTTHandler(void *pvParameters) {
  static unsigned long lastBlinkTime = 0;
  for (;;) {
    Blynk.run();

    if (!client.connected()) {
      reconnect();
    }
    client.loop();

    if (WiFi.status() == WL_CONNECTED) {
      if (millis() - lastBlinkTime > 500) {
        digitalWrite(wifiLed, !digitalRead(wifiLed));
        lastBlinkTime = millis();
      }
    } else {
      digitalWrite(wifiLed, HIGH);
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ==== SETUP ====
void setup() {
  Serial.begin(115200);
  pinMode(callButton, INPUT_PULLUP);
  pinMode(billButton, INPUT_PULLUP);
  pinMode(resetButton, INPUT_PULLUP);
  pinMode(greenLed, OUTPUT);
  pinMode(blueLed, OUTPUT);
  pinMode(wifiLed, OUTPUT);

  digitalWrite(greenLed, LOW);
  digitalWrite(blueLed, LOW);
  digitalWrite(wifiLed, HIGH);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  Blynk.begin(auth, WiFi.SSID().c_str(), WiFi.psk().c_str());

  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  while (time(nullptr) < 100000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n🕐 Waktu sinkron!");

  xTaskCreatePinnedToCore(TaskHandleButtons, "Button Task", 2048, NULL, 1, &TaskButton, 1);
  xTaskCreatePinnedToCore(TaskMQTTHandler, "MQTT Task", 4096, NULL, 1, &TaskMQTT, 1);
}

// ==== LOOP ====
void loop() {
  // FreeRTOS: tidak ada yang dijalankan di sini
}
