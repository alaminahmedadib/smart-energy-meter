#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "ACS712.h"
#include <ZMPT101B.h>

/* ===== BUTTON PINS ===== */
#define BTN1 27
#define BTN2 14
#define BTN3 12

bool state1 = false;
bool state2 = false;
bool state3 = false;

int lastBtn1 = HIGH;
int lastBtn2 = HIGH;
int lastBtn3 = HIGH;

/* ===== WiFi ===== */
const char *ssid = "your wifi name";
const char *password = "your wifi password";

/* ===== MQTT ===== */
const char *mqtt_server = "broker.emqx.io";
const char *subscribeTopic = "home_auto_3_load_sub";
const char *publishTopic = "home_auto_3_load_pub";

WiFiClient espClient;
PubSubClient client(espClient);

/* ===== LCD ===== */
LiquidCrystal_I2C lcd(0x27, 16, 2);

/* ===== Sensors ===== */
ACS712 ACS(34, 3.6, 4095, 150);
ZMPT101B voltageSensor(35, 50.0);

/* ===== Variables ===== */
float kwh = 0;
int v, c;
float w = 0;

/* ===== Load Pins ===== */
#define LOAD1 5
#define LOAD2 18
#define LOAD3 19
#define buzzer 4

unsigned long previousMillis = 0;
const long interval = 10000;

/* ===== Task Handle ===== */
TaskHandle_t buttonTaskHandle;

/* ===== MQTT Callback ===== */
void callback(char *topic, byte *payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.print("MQTT Command: ");
  Serial.println(msg);

  if (msg == "1") digitalWrite(LOAD1, LOW);
  else if (msg == "2") digitalWrite(LOAD1, HIGH);
  else if (msg == "3") digitalWrite(LOAD2, LOW);
  else if (msg == "4") digitalWrite(LOAD2, HIGH);
  else if (msg == "5") digitalWrite(LOAD3, LOW);
  else if (msg == "6") digitalWrite(LOAD3, HIGH);
}

/* ===== MQTT Reconnect ===== */
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting MQTT...");
    if (client.connect("ESP32_Client_Final")) {
      Serial.println("Connected");
      client.subscribe(subscribeTopic);
    } else {
      Serial.print("Failed, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

/* ===== BUTTON TASK ===== */
void buttonTask(void *pvParameters) {
  while (1) {

    int btn1 = digitalRead(BTN1);
    if (btn1 != lastBtn1 && btn1 == LOW) {
      state1 = !state1;
      digitalWrite(LOAD1, state1 ? LOW : HIGH);
      digitalWrite(buzzer, HIGH);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      digitalWrite(buzzer, LOW);
    }
    lastBtn1 = btn1;

    int btn2 = digitalRead(BTN2);
    if (btn2 != lastBtn2 && btn2 == LOW) {
      state2 = !state2;
      digitalWrite(LOAD2, state2 ? LOW : HIGH);
      digitalWrite(buzzer, HIGH);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      digitalWrite(buzzer, LOW);
    }
    lastBtn2 = btn2;

    int btn3 = digitalRead(BTN3);
    if (btn3 != lastBtn3 && btn3 == LOW) {
      state3 = !state3;
      digitalWrite(LOAD3, state3 ? LOW : HIGH);
      digitalWrite(buzzer, HIGH);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      digitalWrite(buzzer, LOW);
    }
    lastBtn3 = btn3;

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LOAD1, OUTPUT);
  pinMode(LOAD2, OUTPUT);
  pinMode(LOAD3, OUTPUT);
  pinMode(buzzer, OUTPUT);

  digitalWrite(LOAD1, HIGH);
  digitalWrite(LOAD2, HIGH);
  digitalWrite(LOAD3, HIGH);

  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);

  /* ===== LCD ===== */
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Energy Meter");

  /* ===== WiFi ===== */
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");

  int tryCount = 0;
  while (WiFi.status() != WL_CONNECTED && tryCount < 20) {
    delay(500);
    Serial.print(".");
    tryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected");
    Serial.println(WiFi.localIP());

    lcd.clear();
    lcd.print("WiFi Connected");
    delay(1000);
  } else {
    Serial.println("\nWiFi Failed");
    lcd.clear();
    lcd.print("WiFi Failed");
    delay(1500);
  }

  /* ===== MQTT ===== */
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  /* ===== Sensors ===== */
  ACS.autoMidPoint();
  voltageSensor.setSensitivity(500.0f);

  xTaskCreatePinnedToCore(buttonTask, "ButtonTask", 2048, NULL, 1, &buttonTaskHandle, 1);
}

void loop() {

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) reconnect();
    client.loop();
  }

  /* ===== CURRENT ===== */
  float avg = 0;
  for (int i = 0; i < 100; i++) avg += ACS.mA_AC();
  float mA = avg / 100.0;
  c = (mA > 20) ? mA : 0;

  /* ===== VOLTAGE ===== */
  float voltage = voltageSensor.getRmsVoltage();
  v = (voltage > 10) ? voltage : 0;

  /* ===== POWER ===== */
  if (c > 0) {
    w = voltage * (c / 1000.0);
    kwh += w / 216000;
  } else {
    w = 0;
  }

  float bill = kwh * 7;

  /* ===== LCD (NO FLICKER) ===== */
  lcd.setCursor(0, 0);
  lcd.print(v);
  lcd.print("V-");
  lcd.print(c);
  lcd.print("mA-");
  lcd.print((int)w);
  lcd.print("W   ");

  lcd.setCursor(0, 1);
  lcd.print(kwh, 2);
  lcd.print("kWh-");
  lcd.print(bill, 2);
  lcd.print("Tk   ");

  delay(1000);

  /* ===== MQTT EVERY 10 SEC ===== */
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {

    previousMillis = currentMillis;

    String payload = String(v) + "," + String(c) + "," + String(w,1) + "," + String(kwh,3) + "," + String(bill,2);

    if (client.connected()) {
      client.publish(publishTopic, payload.c_str());
      Serial.println("Published: " + payload);
    }
  }
}
