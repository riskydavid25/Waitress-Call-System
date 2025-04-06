// Library WiFi dan MQTT
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>

// Library Blynk
#include <BlynkSimpleEsp32.h>

// OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// DFPlayer Mini
#include <DFRobotDFPlayerMini.h>

// JSON untuk parsing data dari sender
#include <ArduinoJson.h>

// OLED display konfigurasi
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DFPlayer Mini pada Serial2 (pin 16 RX, 17 TX)
#define RXD2 16
#define TXD2 17
HardwareSerial mySerial(2);
DFRobotDFPlayerMini dfPlayer;

// MQTT server info
const char *mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char *mqtt_client_id = "ESP32_Receiver";

// Blynk konfigurasi
#define BLYNK_PRINT Serial
#define BLYNK_AUTH "cjqSPyXSlzm32sMLG0JOfH7ANlSvqe8M"
const int wifiLed = 14;  // LED indikator koneksi WiFi

// Status 3 meja: [meja][0 = call, 1 = bill]
bool statusSender[3][2] = {{false, false}, {false, false}, {false, false}};

// Waktu terakhir pesan diterima untuk debounce
unsigned long lastMessageTime[3][2] = {{0}};
const unsigned long debounceDelay = 1000;

WiFiClient espClient;
PubSubClient client(espClient);
WiFiManager wifiManager;

// Fungsi update tampilan OLED
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // Judul
  display.setCursor((SCREEN_WIDTH - 72) / 2, 0);
  display.println("   STATUS MEJA");

  // Menampilkan status 3 meja
  for (int i = 0; i < 3; i++) {
    display.setCursor(0, 16 + i * 16);
    display.printf("M%d C:%c B:%c", i + 1,
      statusSender[i][0] ? '\xFB' : '\xD7',  // ✓ / × untuk Call
      statusSender[i][1] ? '\xFB' : '\xD7'   // ✓ / × untuk Bill
    );
  }

  display.display();
}

// Menangani pesan masuk dari sender dalam format JSON
void handleMessage(const String& senderId, const String& type, bool status, int count) {
  int meja = senderId.substring(6).toInt() - 1;
  int idx = (type == "call") ? 0 : 1;
  unsigned long now = millis();

  if (meja >= 0 && meja < 3 && now - lastMessageTime[meja][idx] > debounceDelay) {
    statusSender[meja][idx] = status;

    if (status) dfPlayer.play(1 + meja * 2 + idx);  // File 1-6 tergantung meja dan jenis
    Blynk.virtualWrite(V1 + meja * 2 + idx, status ? 1 : 0);

    lastMessageTime[meja][idx] = now;
    updateDisplay();
  }
}

// Fungsi callback MQTT
void callback(char *topic, byte *payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("❌ JSON Parse failed: ");
    Serial.println(error.c_str());
    return;
  }

  String senderId = doc["id"] | "";
  String type = doc["type"] | "";
  bool status = doc["status"] | false;
  int count = doc["count"] | 0;

  handleMessage(senderId, type, status, count);
}

// Task untuk handle koneksi dan loop MQTT
void mqttTask(void *parameter) {
  for (;;) {
    if (!client.connected()) {
      while (!client.connected()) {
        if (client.connect(mqtt_client_id)) {
          client.subscribe("waitress/sender1/call");
          client.subscribe("waitress/sender1/bill");
          client.subscribe("waitress/sender2/call");
          client.subscribe("waitress/sender2/bill");
          client.subscribe("waitress/sender3/call");
          client.subscribe("waitress/sender3/bill");
        } else {
          vTaskDelay(3000 / portTICK_PERIOD_MS);
        }
      }
    }
    client.loop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// Task LED indikator WiFi
void ledTask(void *parameter) {
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      digitalWrite(wifiLed, !digitalRead(wifiLed));
      vTaskDelay(500 / portTICK_PERIOD_MS);
    } else {
      digitalWrite(wifiLed, HIGH);
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
  }
}

void setup() {
  Serial.begin(115200);
  mySerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
  pinMode(wifiLed, OUTPUT);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("❌ OLED gagal"));
    while (true);
  }

  // DFPlayer
  if (!dfPlayer.begin(mySerial)) {
    Serial.println("❌ DFPlayer gagal");
  } else {
    dfPlayer.volume(30);
    dfPlayer.play(7);  // 0007.mp3 = intro
  }

  // Splash screen
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20, 0);
  display.println("WAITRESS");
  display.setCursor(40, 20);
  display.println("CALL");
  display.setCursor(34, 40);
  display.println("SYSTEM");
  display.display();
  delay(2000);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 20);
  display.println("RISKY DAVID K");
  display.setCursor(30, 35);
  display.println("2212101134");
  display.display();
  delay(2000);

  updateDisplay();

  // Koneksi WiFi via WiFiManager
  wifiManager.setTimeout(180);
  if (!wifiManager.autoConnect("Receiver_AP")) {
    Serial.println("⛔ Timeout WiFi");
    while (true) {
      digitalWrite(wifiLed, HIGH);
      delay(500);
    }
  }

  // MQTT dan Blynk
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  Blynk.begin(BLYNK_AUTH, WiFi.SSID().c_str(), WiFi.psk().c_str());

  xTaskCreate(mqttTask, "MQTT Task", 4096, NULL, 1, NULL);
  xTaskCreate(ledTask, "LED Task", 1024, NULL, 1, NULL);
}

void loop() {
  Blynk.run();
}
