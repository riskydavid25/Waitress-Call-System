#include <WiFiManager.h> 
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DFRobotDFPlayerMini.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>

// === OLED Setup ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === DFPlayer Setup ===
HardwareSerial mySerial(2);  // RX = 16, TX = 17
DFRobotDFPlayerMini dfPlayer;

// === MQTT Setup ===
WiFiClient espClient;
PubSubClient client(espClient);
const char *mqtt_server = "broker.emqx.io";

// === Status Variabel ===
String statusSender[6] = {"OFF", "OFF", "OFF", "OFF", "OFF", "OFF"};
unsigned long lastMessageTime[6] = {0, 0, 0, 0, 0, 0};
const unsigned long debounceDelay = 1000;

// === LED WiFi Status ===
#define LED_WIFI 14
bool wifiConnected = false;
unsigned long lastBlink = 0;
bool ledState = false;

// === Setup Display ===
void setupDisplay() {
  Wire.begin(21, 22);  // I2C pin ESP32

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED Gagal Inisialisasi");
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(20, 0);
  display.println("WAITRESS");
  display.setCursor(40, 20);
  display.println("CALL");
  display.setCursor(34, 40);
  display.println("SYSTEM");
  display.display();
  delay(3000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 20);
  display.println("RISKY DAVID K");
  display.setCursor(30, 35);
  display.println("2212101134");
  display.display();
  delay(2000);
}

// === Update OLED Display ===
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  for (int i = 0; i < 6; i++) {
    display.setCursor(0, i * 10);
    display.printf("M%d (%s): %s", (i / 2) + 1, (i % 2 == 0 ? "Call" : "Bill"), statusSender[i].c_str());
  }
  display.display();
}

// === MQTT Callback ===
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("[MQTT] Topic: ");
  Serial.println(topic);

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("[ERROR] JSON: ");
    Serial.println(error.c_str());
    return;
  }

  String senderId = doc["id"];
  String type = doc["type"];
  bool status = doc["status"];
  unsigned long currentMillis = millis();

  int index = -1;
  if (senderId == "ESP32_Sender1" && type == "call") index = 0;
  else if (senderId == "ESP32_Sender1" && type == "bill") index = 1;
  else if (senderId == "ESP32_Sender2" && type == "call") index = 2;
  else if (senderId == "ESP32_Sender2" && type == "bill") index = 3;
  else if (senderId == "ESP32_Sender3" && type == "call") index = 4;
  else if (senderId == "ESP32_Sender3" && type == "bill") index = 5;

  Serial.printf("Parsed: ID=%s, TYPE=%s, STATUS=%s, index=%d\n", senderId.c_str(), type.c_str(), status ? "true" : "false", index);

  if (index != -1 && currentMillis - lastMessageTime[index] > debounceDelay) {
    statusSender[index] = status ? "ON" : "OFF";
    updateDisplay();

    if (status) {
      dfPlayer.play(index + 1);
    }

    lastMessageTime[index] = currentMillis;
  }
}

// === Reconnect MQTT ===
void reconnect() {
  while (!client.connected()) {
    Serial.print("[MQTT] Menghubungkan...");
    if (client.connect("ESP32_Receiver")) {
      Serial.println("Tersambung!");
      client.subscribe("waitress/+/call");
      client.subscribe("waitress/+/bill");
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" coba lagi 5 detik...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_WIFI, OUTPUT);
  digitalWrite(LED_WIFI, HIGH);  // LED nyala terus sebelum WiFi connect

  mySerial.begin(9600, SERIAL_8N1, 16, 17);
  if (!dfPlayer.begin(mySerial)) {
    Serial.println("[DFPlayer] Gagal inisialisasi!");
    while (1);
  }
  dfPlayer.volume(25);
  
  setupDisplay();

  WiFiManager wm;
  wm.setConfigPortalTimeout(60);
  if (!wm.autoConnect("WaitressReceiver")) {
    Serial.println("Gagal connect WiFi. Restart...");
    ESP.restart();
  }
  Serial.println("[WiFi] Terkoneksi: " + WiFi.localIP().toString());
  wifiConnected = true;

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (wifiConnected) {
    if (millis() - lastBlink >= 500) {
      lastBlink = millis();
      ledState = !ledState;
      digitalWrite(LED_WIFI, ledState);
    }
  } else {
    digitalWrite(LED_WIFI, HIGH);
  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
