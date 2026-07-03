// ================================================================
//  SERRE'TECH — Code fusionné : Capteurs + MQTT + LED Matrix
// ================================================================

#include <Wire.h>
#include "DHT.h"
#include <Adafruit_BMP085.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <MHZ19.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiS3.h>
#include <PubSubClient.h>
#include <Arduino_LED_Matrix.h>

// ================================================================
//  CONFIGURATION WI-FI
// ================================================================
const char* ssid     = "ICAM_DEPANNAGE";
const char* password = "Ica@mVannes56";
//const char* ssid     = "NETGEAR-3V";
//const char* password = "E%V&il*m$1";

// ================================================================
//  CONFIGURATION MQTT
// ================================================================
const char* MQTT_BROKER    = "mqtt.dev.icam.school";
const int   MQTT_PORT      = 1883;
const char* MQTT_TOPIC_ROOT = "bzh/mecatro/dashboard/SerreTech/";
const char* MQTT_CLIENT_ID  = "SerreTechClientR4";

// Topics par variable
const char* MQTT_TOPIC_LIGHT1      = "Lumière 1";
const char* MQTT_TOPIC_LIGHT2      = "Lumière 2";
const char* MQTT_TOPIC_DHT1TEMP    = "Température capteur 1";
const char* MQTT_TOPIC_DHT1HUM     = "Humidité capteur 1";
const char* MQTT_TOPIC_DHT2TEMP    = "Température capteur 2";   // fixé à 0
const char* MQTT_TOPIC_DHT2HUM     = "Humidité capteur 2";      // fixé à 0
const char* MQTT_TOPIC_BMPPRESSURE = "Pression";
const char* MQTT_TOPIC_SONDE1TEMP  = "Température sonde 1";
const char* MQTT_TOPIC_SONDE2TEMP  = "Température sonde 2";
const char* MQTT_TOPIC_SOILHUM     = "Humidité terre";
const char* MQTT_TOPIC_CO2TEMP     = "Température capteur Co2";
const char* MQTT_TOPIC_CO2         = "Taux de Co2";

// ================================================================
//  OLED
// ================================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================================================================
//  CAPTEURS
// ================================================================
#define BH1      0x23       // Capteur lumière 1
#define BH2      0x5C       // Capteur lumière 2
#define DHTPIN   2          // DHT Humidité / Température
#define DHTTYPE  DHT11

#define SONDE1   3          // Sonde temp Dallas 1
#define SONDE2   4          // Sonde temp Dallas 2

DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP085 bmp;
//MHZ19 myMHZ19;

OneWire oneWire1(SONDE1);
OneWire oneWire2(SONDE2);
DallasTemperature sensor1(&oneWire1);
DallasTemperature sensor2(&oneWire2);

// ================================================================
//  LED MATRIX
// ================================================================
ArduinoLEDMatrix matrix;

// Icône succès (feuille)
uint8_t customIcon[8][12] = {
  { 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0 },
  { 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0 },
  { 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0 },
  { 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0 },
  { 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

// Icône erreur (croix)
uint8_t errorIcon[8][12] = {
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
  { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 },
  { 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0 },
  { 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0 },
  { 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0 },
  { 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0 },
  { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 },
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }
};

// ================================================================
//  RÉSEAU
// ================================================================
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

// ================================================================
//  BTS7960 — VERRINS PORTE
// ================================================================
const int RPWM = 5;
const int LPWM = 6;

enum VerrinState { VERRIN_IDLE, VERRIN_OPENING, VERRIN_CLOSING_1, VERRIN_CLOSING_2 };
VerrinState   verrinState     = VERRIN_IDLE;
unsigned long verrinStepStart = 0;

// ================================================================
//  RELAI — CHAUFFAGE
// ================================================================
const int PIN_RELAI = 7;

// ================================================================
//  L298N — VENTILATION (Moteur 1) & ARROSAGE/POMPE (Moteur 2)
//  ⚠️ Pins 10/11/12/13 réservés WiFiS3 → remappés sur pins libres
// ================================================================
const int ENA = 9;
const int IN1 = 8;
const int IN2 = 10;

const int ENB = 11;
const int IN3 = 12;
const int IN4 = 13;

// ================================================================
//  ÉTAT DES ACTIONNEURS (mis à jour par les commandes du site web)
// ================================================================
String cmd_porte       = "INCONNU";
String cmd_ventilation = "OFF";
String cmd_chauffage   = "OFF";
String cmd_arrosage    = "OFF";
float  cmd_temp_cible  = 0.0;
bool   tempAutoMode    = false;  // true dès qu'une cible est reçue

// ================================================================
//  TIMING — 10 s entre chaque envoi (> 5 s min imposé)
// ================================================================
unsigned long previousMillis  = 0;
const long    publishInterval = 10000;

// ================================================================
//  HELPERS
// ================================================================

// --- Barre de progression sur la LED matrix ---
void updateProgressBar(unsigned long elapsed, unsigned long total) {
  uint8_t frame[8][12] = {};
  int pixelsOn = (elapsed * 8) / total;
  if (pixelsOn > 8) pixelsOn = 8;
  for (int i = 0; i < pixelsOn; i++) {
    frame[7 - i][0] = 1;
  }
  matrix.renderBitmap(frame, 8, 12);
}

// --- Construction du topic complet et publication ---
void publishValue(const char* topicSuffix, const char* payload) {
  char fullTopic[100];
  snprintf(fullTopic, sizeof(fullTopic), "%s%s", MQTT_TOPIC_ROOT, topicSuffix);
  if (!mqttClient.publish(fullTopic, payload)) {
    Serial.print("Échec envoi : ");
    Serial.println(fullTopic);
  }
}

// --- Reconnexion MQTT ---
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connexion MQTT... ");
    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      Serial.println("OK");
      mqttClient.subscribe("bzh/mecatro/projets/SerreTech/actionneurs/#");
    } else {
      Serial.print("Échec (code ");
      Serial.print(mqttClient.state());
      Serial.println("), nouvelle tentative dans 5 s");
      matrix.renderBitmap(errorIcon, 8, 12);
      delay(5000);
    }
  }
}

// --- Callback MQTT — réception des commandes du site web ---
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];

  String t = String(topic);

  if (t.endsWith("/porte")) {
    if (message != cmd_porte) {
      cmd_porte = message;
      Serial.println("[CMD] Porte → " + cmd_porte);
      if      (cmd_porte == "OPEN")  verrinOuvrir();
      else if (cmd_porte == "CLOSE") verrinFermer();
    }
  } else if (t.endsWith("/temp_cible")) {
    float val = message.toFloat();
    if (val > 0 && val != cmd_temp_cible) {
      cmd_temp_cible = val;
      tempAutoMode   = true;
      Serial.println("[THERMO] Cible → " + String(cmd_temp_cible, 1) + " °C (régulation active)");
    }
  } else if (t.endsWith("/ventilation")) {
    if (message != cmd_ventilation) {
      cmd_ventilation = message;
      if (cmd_ventilation == "ON") ventilationOn();
      else                          ventilationOff();
      Serial.println("[CMD] Ventilation → " + cmd_ventilation);
    }
  } else if (t.endsWith("/chauffage")) {
    if (message != cmd_chauffage) {
      cmd_chauffage = message;
      digitalWrite(PIN_RELAI, cmd_chauffage == "HOT" ? HIGH : LOW);
      Serial.println("[CMD] Chauffage → " + cmd_chauffage);
    }
  } else if (t.endsWith("/arrosage")) {
    if (message != cmd_arrosage) {
      cmd_arrosage = message;
      if (cmd_arrosage == "ON") arrosageOn();
      else                       arrosageOff();
      Serial.println("[CMD] Arrosage → " + cmd_arrosage);
    }
  } else {
    Serial.println("[CMD] Topic inconnu : " + t + " = " + message);
  }
}

// ================================================================
//  VERRIN helpers
// ================================================================
void verrinStop() {
  analogWrite(RPWM, 0);
  analogWrite(LPWM, 0);
  verrinState = VERRIN_IDLE;
  Serial.println("[VERRIN] Arrêt");
}

void verrinOuvrir() {
  analogWrite(RPWM, 0);
  analogWrite(LPWM, 255);
  verrinState     = VERRIN_OPENING;
  verrinStepStart = millis();
  Serial.println("[VERRIN] Ouverture...");
}

void verrinFermer() {
  analogWrite(RPWM, 255);
  analogWrite(LPWM, 0);
  verrinState     = VERRIN_CLOSING_1;
  verrinStepStart = millis();
  Serial.println("[VERRIN] Fermeture phase 1...");
}

// ================================================================
//  RÉGULATION TEMPÉRATURE AUTOMATIQUE
// ================================================================
const float TEMP_HYST = 0.5;  // ± 0.5 °C d'hystérésis

void regulateTemperature(float currentTemp) {
  if (!tempAutoMode) return;

  if (currentTemp < cmd_temp_cible - TEMP_HYST) {
    // Trop froid → chauffage ON, ventilation OFF
    if (cmd_chauffage != "HOT") {
      cmd_chauffage = "HOT";
      digitalWrite(PIN_RELAI, HIGH);
      Serial.println("[THERMO] Trop froid (" + String(currentTemp, 1) + "°C) → Chauffage ON");
    }
    if (cmd_ventilation != "OFF") {
      cmd_ventilation = "OFF";
      ventilationOff();
    }
  } else if (currentTemp > cmd_temp_cible + TEMP_HYST) {
    // Trop chaud → ventilation ON, chauffage OFF
    if (cmd_chauffage != "OFF") {
      cmd_chauffage = "OFF";
      digitalWrite(PIN_RELAI, LOW);
      Serial.println("[THERMO] Trop chaud (" + String(currentTemp, 1) + "°C) → Chauffage OFF");
    }
    if (cmd_ventilation != "ON") {
      cmd_ventilation = "ON";
      ventilationOn();
      Serial.println("[THERMO] Trop chaud → Ventilation ON");
    }
  } else {
    // Dans la plage cible → tout éteindre
    if (cmd_chauffage != "OFF") {
      cmd_chauffage = "OFF";
      digitalWrite(PIN_RELAI, LOW);
      Serial.println("[THERMO] Cible atteinte → Chauffage OFF");
    }
    if (cmd_ventilation != "OFF") {
      cmd_ventilation = "OFF";
      ventilationOff();
      Serial.println("[THERMO] Cible atteinte → Ventilation OFF");
    }
  }
}

// ================================================================
//  L298N helpers
// ================================================================
void ventilationOn() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(ENA, HIGH);
  Serial.println("[L298N] Ventilation ON");
}

void ventilationOff() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(ENA, LOW);
  Serial.println("[L298N] Ventilation OFF");
}

void arrosageOn() {
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  digitalWrite(ENB, HIGH);
  Serial.println("[L298N] Arrosage ON");
}

void arrosageOff() {
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  digitalWrite(ENB, LOW);
  Serial.println("[L298N] Arrosage OFF");
}

// ================================================================
//  BH1750 helpers
// ================================================================
void initBH(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(0x10); // continuous high resolution
  Wire.endTransmission();
}

float readBH(uint8_t addr) {
  Wire.requestFrom(addr, (uint8_t)2);
  return (Wire.read() << 8 | Wire.read()) / 1.2;
}

// ================================================================
//  SETUP
// ================================================================
void setup() {
  Serial.begin(9600);
  Wire.begin();
  matrix.begin();

  // --- Relai chauffage ---
  pinMode(PIN_RELAI, OUTPUT);
  digitalWrite(PIN_RELAI, LOW);

  // --- BTS7960 verrins ---
  pinMode(RPWM, OUTPUT);
  pinMode(LPWM, OUTPUT);
  analogWrite(RPWM, 0);
  analogWrite(LPWM, 0);

  // --- L298N ventilation & arrosage ---
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  ventilationOff();
  arrosageOff();

  // --- Capteurs ---
  dht.begin();
  initBH(BH1);
  initBH(BH2);
  sensor1.begin();
  sensor2.begin();

  if (!bmp.begin()) {
    Serial.println("BMP180 erreur !");
  } else {
    Serial.println("BMP180 OK");
  }

  // --- OLED ---
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED erreur");
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("SERRE'TECH");
  display.display();
  delay(2000);

  // --- MH-Z19B sur Serial1 (pins 0/1) ---
  //Serial1.begin(9600);
  //myMHZ19.begin(Serial1);
  //myMHZ19.autoCalibration(false);

  // --- Wi-Fi ---
  Serial.print("Connexion Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connecté");

  // --- MQTT ---
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(callback);
}

// ================================================================
//  LOOP
// ================================================================
void loop() {

  // ---- Machine à états verrins ----
  if (verrinState != VERRIN_IDLE) {
    unsigned long elapsed = millis() - verrinStepStart;
    if (verrinState == VERRIN_OPENING && elapsed >= 11000) {
      verrinStop();
      Serial.println("[VERRIN] Ouverture terminée");
    } else if (verrinState == VERRIN_CLOSING_1 && elapsed >= 11000) {
      analogWrite(RPWM, 150);
      analogWrite(LPWM, 0);
      verrinState     = VERRIN_CLOSING_2;
      verrinStepStart = millis();
      Serial.println("[VERRIN] Fermeture phase 2...");
    } else if (verrinState == VERRIN_CLOSING_2 && elapsed >= 6000) {
      verrinStop();
      Serial.println("[VERRIN] Fermeture terminée");
    }
  }

  // ---- Vérification Wi-Fi ----
  if (WiFi.status() != WL_CONNECTED) {
    matrix.renderBitmap(errorIcon, 8, 12);
    WiFi.begin(ssid, password);
    delay(1000);
    return;
  }

  // ---- Vérification MQTT ----
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  // ---- Timing ----
  unsigned long currentMillis = millis();
  unsigned long timeElapsed   = currentMillis - previousMillis;

  if (timeElapsed < publishInterval) {
    updateProgressBar(timeElapsed, publishInterval);
    return; // On attend la prochaine fenêtre de publication
  }

  previousMillis = currentMillis;

  // ==============================================================
  //  LECTURE DE TOUS LES CAPTEURS
  // ==============================================================

  // --- BH1750 (lumière) ---
  float lux1 = readBH(BH1);
  float lux2 = readBH(BH2);
  Serial.print("BH1750: "); Serial.print(lux1); Serial.print(" | "); Serial.println(lux2);

  // --- DHT11 (température / humidité air) ---
  float dht1_hum  = dht.readHumidity();
  float dht1_temp = dht.readTemperature();
  if (isnan(dht1_hum) || isnan(dht1_temp)) {
    Serial.println("Erreur DHT11 !");
    dht1_hum = 0; dht1_temp = 0;
  }
  Serial.print("DHT11: "); Serial.print(dht1_temp); Serial.print(" °C | ");
  Serial.print(dht1_hum); Serial.println(" %");

  // --- BMP180 (pression / température) ---
  float bmp_temp     = bmp.readTemperature();
  float bmp_pressure = bmp.readPressure();
  Serial.print("BMP: "); Serial.print(bmp_temp); Serial.print(" °C | ");
  Serial.print(bmp_pressure); Serial.println(" Pa");

  // --- Capteur eau ---
  int water = analogRead(A0);
  Serial.print("Capteur eau: "); Serial.println(water);

  // --- Sondes Dallas (température sol/eau) ---
  sensor1.requestTemperatures();
  sensor2.requestTemperatures();
  float t1 = sensor1.getTempCByIndex(0);
  float t2 = sensor2.getTempCByIndex(0);
  if (t1 < 0) t1 = 0;
  if (t2 < 0) t2 = 0;
  Serial.print("Sonde 1: "); Serial.print(t1); Serial.print(" °C | ");
  Serial.print("Sonde 2: "); Serial.println(t2);

  regulateTemperature(t1);

  // --- Humidité sol ---
  int soilHum = analogRead(A1);
  float soilHumPercent = 100.0 * (570.0 - soilHum) / (570.0 - 60.0);
  soilHumPercent = constrain(soilHumPercent, 0, 100);
  Serial.print("Humidité sol: "); Serial.println(soilHumPercent);

  // --- CO2 MH-Z19 ---
  //int co2  = myMHZ19.getCO2();
  //int co2T = myMHZ19.getTemperature();
  //Serial.print("CO2: "); Serial.print(co2); Serial.print(" ppm | T: "); Serial.println(co2T);

  Serial.println();

  // ==============================================================
  //  PUBLICATION MQTT
  // ==============================================================
  char buf[24];

  // Lumière 1 (lux)
  snprintf(buf, sizeof(buf), "%d lux", (int)lux1);
  publishValue(MQTT_TOPIC_LIGHT1, buf);

  // Lumière 2 (lux)
  snprintf(buf, sizeof(buf), "%d lux", (int)lux2);
  publishValue(MQTT_TOPIC_LIGHT2, buf);

  // Température DHT11
  snprintf(buf, sizeof(buf), "%.1f°C", dht1_temp);
  publishValue(MQTT_TOPIC_DHT1TEMP, buf);

  // Humidité DHT11
  snprintf(buf, sizeof(buf), "%.0f%%", dht1_hum);
  publishValue(MQTT_TOPIC_DHT1HUM, buf);

  // DHT2 — pas de capteur pour l'instant, on envoie 0
  publishValue(MQTT_TOPIC_DHT2TEMP, "0°C");
  publishValue(MQTT_TOPIC_DHT2HUM,  "0%");

  // Pression BMP180 (hPa)
  snprintf(buf, sizeof(buf), "%d hPa", (int)(bmp_pressure / 100));
  publishValue(MQTT_TOPIC_BMPPRESSURE, buf);

  // Sondes Dallas
  snprintf(buf, sizeof(buf), "%.1f°C", t1);
  publishValue(MQTT_TOPIC_SONDE1TEMP, buf);

  snprintf(buf, sizeof(buf), "%.1f°C", t2);
  publishValue(MQTT_TOPIC_SONDE2TEMP, buf);

  // Humidité sol
  snprintf(buf, sizeof(buf), "%.0f%%", soilHumPercent);
  publishValue(MQTT_TOPIC_SOILHUM, buf);

  // Température capteur CO2
  //snprintf(buf, sizeof(buf), "%d°C", co2T);
  //publishValue(MQTT_TOPIC_CO2TEMP, buf);

  // Taux CO2 (ppm)
  //snprintf(buf, sizeof(buf), "%d ppm", co2);
  //publishValue(MQTT_TOPIC_CO2, buf);

  // ==============================================================
  //  OLED — même affichage qu'avant
  // ==============================================================
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);

  display.print("Li1: ");   display.print((int)lux1);
  display.print(" | Li2: ");display.println((int)lux2);

  display.print("T1: ");    display.print(dht1_temp, 1);
  display.print(" | H1: "); display.println(dht1_hum, 0);

  display.print("Pressure: ");
  display.print((int)(bmp_pressure / 100));
  display.println(" hPa");

  display.print("S1: ");    display.print(t1, 1);
  display.print(" | S2:"); display.println(t2, 1);

  display.print("Soil hum: ");
  display.print(soilHumPercent, 0);
  display.println("%");

  //display.print("CO2: ");   display.print(co2);
  //display.print("ppm | T:");display.print(co2T);
  //display.println("C");

  display.display();

  // ==============================================================
  //  LED MATRIX — icône succès brièvement
  // ==============================================================
  matrix.renderBitmap(customIcon, 8, 12);
  delay(1500);
}
