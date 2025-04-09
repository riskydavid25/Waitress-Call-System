#include <WiFiManager.h> 
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DFRobotDFPlayerMini.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include <BlynkSimpleEsp32.h>

// === Blynk Setup ===
#define BLYNK_TEMPLATE_ID "TMPLXXXXXX"
#define BLYNK_DEVICE_NAME "Waitress Receiver"
#define BLYNK_AUTH_TOKEN "YOUR_AUTH_TOKEN"

// Virtual pins sesuai permintaan
#define BLYNK_SENDER1_CALL V1
#define BLYNK_SENDER1_BILL V2
#define BLYNK_RESET_SENDER1 V3
#define BLYNK_SENDER2_CALL V4
#define BLYNK_SENDER2_BILL V5
#define BLYNK_RESET_SENDER2 V6
#define BLYNK_SENDER3_CALL V7
#define BLYNK_SENDER3_BILL V8
#define BLYNK_RESET_SENDER3 V9

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

// === Status Variables ===
String statusSender[6] = {"OFF", "OFF", "OFF", "OFF", "OFF", "OFF"};
unsigned long lastMessageTime[6] = {0, 0, 0, 0, 0, 0};
const unsigned long debounceDelay = 1000;

// === LED WiFi Status ===
#define LED_WIFI 14
bool wifiConnected = false;
unsigned long lastBlink = 0;
bool ledState = false;

void setupDisplay() {
  Wire.begin(21, 22);
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

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  for (int i = 0; i < 6; i++) {
    display.setCursor(0, i * 10);
    display.printf("M%d (%s): %s", (i / 2) + 1, (i % 2 == 0 ? "Call" : "Bill"), statusSender[i].c_str());
  }
  display.display();
}

void updateBlynk() {
  // Update status untuk semua sender ke Blynk
  Blynk.virtualWrite(BLYNK_SENDER1_CALL, statusSender[0] == "ON" ? 1 : 0);
  Blynk.virtualWrite(BLYNK_SENDER1_BILL, statusSender[1] == "ON" ? 1 : 0);
  Blynk.virtualWrite(BLYNK_SENDER2_CALL, statusSender[2] == "ON" ? 1 : 0);
  Blynk.virtualWrite(BLYNK_SENDER2_BILL, statusSender[3] == "ON" ? 1 : 0);
  Blynk.virtualWrite(BLYNK_SENDER3_CALL, statusSender[4] == "ON" ? 1 : 0);
  Blynk.virtualWrite(BLYNK_SENDER3_BILL, statusSender[5] == "ON" ? 1 : 0);
}

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
    updateBlynk();

    if (status) {
      dfPlayer.play(index + 1);
    }

    lastMessageTime[index] = currentMillis;
  }
}

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

BLYNK_WRITE(BLYNK_RESET_SENDER1) {
  if (param.asInt() == 1) {
    resetSender("ESP32_Sender1");
    Blynk.virtualWrite(BLYNK_RESET_SENDER1, 0);
  }
}

BLYNK_WRITE(BLYNK_RESET_SENDER2) {
  if (param.asInt() == 1) {
    resetSender("ESP32_Sender2");
    Blynk.virtualWrite(BLYNK_RESET_SENDER2, 0);
  }
}

BLYNK_WRITE(BLYNK_RESET_SENDER3) {
  if (param.asInt() == 1) {
    resetSender("ESP32_Sender3");
    Blynk.virtualWrite(BLYNK_RESET_SENDER3, 0);
  }
}

void resetSender(String senderId) {
  Serial.println("Resetting " + senderId);
  
  String topic = "waitress/" + senderId + "/reset";
  StaticJsonDocument<128> doc;
  doc["command"] = "reset";
  char buffer[128];
  serializeJson(doc, buffer);
  client.publish(topic.c_str(), buffer);
  
  int startIndex = -1;
  if (senderId == "ESP32_Sender1") startIndex = 0;
  else if (senderId == "ESP32_Sender2") startIndex = 2;
  else if (senderId == "ESP32_Sender3") startIndex = 4;
  
  if (startIndex != -1) {
    for (int i = startIndex; i < startIndex + 2; i++) {
      statusSender[i] = "OFF";
    }
    updateDisplay();
    updateBlynk();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_WIFI, OUTPUT);
  digitalWrite(LED_WIFI, HIGH);

  mySerial.begin(9600, SERIAL_8N1, 16, 17);
  if (!dfPlayer.begin(mySerial)) {
    Serial.println("[DFPlayer] Gagal inisialisasi!");
    while (1);
  }
  dfPlayer.volume(30);
  
  setupDisplay();

  WiFiManager wm;
  wm.setConfigPortalTimeout(60);
  if (!wm.autoConnect("WaitressReceiver")) {
    Serial.println("Gagal connect WiFi. Restart...");
    ESP.restart();
  }
  Serial.println("[WiFi] Terkoneksi: " + WiFi.localIP().toString());
  wifiConnected = true;

  Blynk.begin(BLYNK_AUTH_TOKEN, WiFi.SSID().c_str(), WiFi.psk().c_str());
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
  Blynk.run();
}
