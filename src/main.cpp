// === Biblioteker ===
#include <Arduino.h>        // Grundlæggende Arduino-funktioner
#include <WiFi.h>           // Wi-Fi forbindelse til ESP32
#include <PubSubClient.h>   // MQTT klient til kommunikation
#include <time.h>           // Tidsbibliotek til at hente NTP-tid
#include "esp_sleep.h"      // ESP32 deep sleep funktionalitet

// === Wi-Fi netværksoplysninger ===
const char* ssid = "IoT_H3/4";          // Navn på Wi-Fi
const char* password = "98806829";      // Adgangskode til Wi-Fi

// === MQTT serverkonfiguration ===
const char* mqtt_server = "192.168.0.130";   // MQTT server IP (lokalt netværk)
const int mqtt_port = 1883;                  // Standard MQTT port
const char* mqtt_user = "device13";          // Brugernavn
const char* mqtt_pass = "device13-password"; // Kodeord
const char* mqtt_topic = "sensor/device13";  // Emne hvor beskeder sendes

// === Klienter til netværk og MQTT ===
WiFiClient wifiClient;               // Netværksklient til WiFi
PubSubClient mqttClient(wifiClient); // MQTT klient, der bruger WiFi

// === Konfiguration af knapper og LED’er ===
const int buttonPins[] = {27, 26, 25, 33}; // GPIO pins til knapper
const int ledPins[] = {5, 18, 19, 21};     // GPIO pins til LED’er
const int numButtons = 4;                  // Antal knapper og LED’er

// === Tidsindstillinger ===
const unsigned long ledDuration = 7000;         // Hvor længe en LED er tændt (7 sekunder)
const unsigned long stayAwakeDuration = 60000;  // Hvor længe ESP’en forbliver vågen (1 minut)

// === Variabler som husker tilstand mellem dyb søvn ===
RTC_DATA_ATTR bool activePeriod = false;  // Gemmer om vi er i en aktiv periode
unsigned long awakeStart = 0;             // Tidspunkt hvor ESP vågnede

// === Variabler til LED og knapstyring ===
int lastButton = -1;             // Index af sidst trykkede knap
unsigned long ledStart = 0;      // Tidspunkt hvor LED blev tændt
bool ledOn = false;              // Status for LED

// === Funktionsdeklarationer ===
void connectWiFi();
void connectMQTT();
void connectWiFiAndSyncTime();
void goToDeepSleep();
void printFeedbackMessage(int index);
void publishMQTT(int index);

// === Opsætningsfunktion – køres én gang ved opstart eller vækning ===
void setup() {
  Serial.begin(115200); // Start seriel kommunikation til debug
  delay(100);

  // Initialiser GPIO pins
  for (int i = 0; i < numButtons; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);  // Indgang med pull-up modstand
    pinMode(ledPins[i], OUTPUT);           // Udgang til LED
    digitalWrite(ledPins[i], LOW);         // Start med LED slukket
  }

  // Find årsag til opvågning (f.eks. GPIO)
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  awakeStart = millis(); // Husk tid for vågning

  // Tjek om en knap blev trykket under opvågning
  for (int i = 0; i < numButtons; i++) {
    if (digitalRead(buttonPins[i]) == LOW) {
      delay(20); // Debounce: Vent for at undgå falske tryk
      if (digitalRead(buttonPins[i]) == LOW) {
        // Knap bekræftet
        lastButton = i;
        activePeriod = true;
        digitalWrite(ledPins[i], HIGH);  // Tænd LED
        ledStart = millis();
        ledOn = true;

        Serial.printf("Knap %d trykket\n", i + 1);
        printFeedbackMessage(i);        // Print brugervenlig feedback

        connectWiFiAndSyncTime();       // Forbind og hent tid
        connectMQTT();                  // Forbind til MQTT
        publishMQTT(i);                 // Send knapdata
        break; // Stop loop efter første fundne knap
      }
    }
  }
}

// === Løbende hovedloop – kører så længe ESP er vågen ===
void loop() {
  if (!activePeriod) goToDeepSleep(); // Hvis vi ikke er i aktiv tilstand, gå i dvale

  // Gå i dvale efter bestemt tid uden aktivitet
  if (millis() - awakeStart >= stayAwakeDuration) {
    Serial.println("⏳ Tid udløbet – deep sleep");
    goToDeepSleep();
  }

  // Sluk LED efter dens visningstid
  if (ledOn && millis() - ledStart >= ledDuration) {
    for (int i = 0; i < numButtons; i++) {
      digitalWrite(ledPins[i], LOW);
    }
    ledOn = false;
    Serial.println("🔕 LED slukket");
  }

  // Tjek for nye knaptryk i aktiv periode
  for (int i = 0; i < numButtons; i++) {
    if (digitalRead(buttonPins[i]) == LOW) {
      delay(20);
      if (digitalRead(buttonPins[i]) == LOW && !ledOn) {
        // Behandl knaptryk
        lastButton = i;
        digitalWrite(ledPins[i], HIGH);
        ledStart = millis();
        ledOn = true;

        Serial.printf("🔁 Knap %d trykket\n", i + 1);
        printFeedbackMessage(i);
        connectWiFiAndSyncTime();
        connectMQTT();
        publishMQTT(i);
        break;
      }
    }
  }
}

// === WiFi forbindelse ===
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("🔌 Tilslutter WiFi");
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    Serial.print(".");
    delay(500);
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi forbundet!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi fejl.");
  }
}

// === Forbind til WiFi og synkroniser tid via NTP ===
void connectWiFiAndSyncTime() {
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) return;

  configTzTime("CET-1CEST,M3.5.0/02,M10.5.0/03", "pool.ntp.org"); // Dansk sommertid
  Serial.print("⏳ Synkroniserer tid");
  int tries = 0;
  while (time(nullptr) < 100000 && tries < 20) {
    Serial.print(".");
    delay(500);
    tries++;
  }
  Serial.println();

  // Print den aktuelle tid hvis den blev hentet korrekt
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.print("🕒 Tid: ");
    Serial.println(buf);
  } else {
    Serial.println("❌ Kunne ikke hente tid.");
  }
}

// === Forbind til MQTT server ===
void connectMQTT() {
  mqttClient.setServer(mqtt_server, mqtt_port);

  if (!mqttClient.connected()) {
    Serial.print("🔌 Forbinder til MQTT");
    if (mqttClient.connect("device13", mqtt_user, mqtt_pass)) {
      Serial.println(" ✅");
    } else {
      Serial.print(" ❌ Fejl: ");
      Serial.println(mqttClient.state());
    }
  }
}

// === Send knaptryk med timestamp til MQTT ===
void publishMQTT(int index) {
  time_t now = time(nullptr);
  struct tm timeinfo;
  char timeStr[32] = "";
  const char* feedbackText = "";
  const char* timezone = "CET/CEST"; // Sæt manuelt den korrekte tidszone

  // Hent lokal tid
  if (getLocalTime(&timeinfo)) {
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
  } else {
    strcpy(timeStr, "ukendt tidspunkt");
  }

  // Bestem feedbacktekst ud fra knap
  switch (index) {
    case 0: feedbackText = "😕 Lidt dårlig feedback"; break;
    case 1: feedbackText = "😐 Middel feedback"; break;
    case 2: feedbackText = "😊 God feedback"; break;
    case 3: feedbackText = "😠 Meget dårlig feedback"; break;
    default: feedbackText = "Ukendt feedback"; break;
  }

  // Byg JSON-payload med tidszone
  String payload = "{";
  payload += "\"button\":" + String(index) + ",";
  payload += "\"feedback\":\"" + String(feedbackText) + "\",";
  payload += "\"timestamp\":" + String(now) + ",";
  payload += "\"time_str\":\"" + String(timeStr) + "\",";
  payload += "\"timezone\":\"" + String(timezone) + "\"";
  payload += "}";

  // Send som MQTT
  bool success = mqttClient.publish(mqtt_topic, payload.c_str());

  if (success) {
    Serial.println("📤 JSON sendt til MQTT: " + payload);
  } else {
    Serial.println("❌ MQTT publish fejlede.");
  }
}




// === Print brugerfeedback i klartekst ===
void printFeedbackMessage(int index) {
  switch (index) {
    case 0: Serial.println("😕 Lidt dårlig feedback"); break;
    case 1: Serial.println("😐 Middel feedback"); break;
    case 2: Serial.println("😊 God feedback"); break;
    case 3: Serial.println("😠 Meget dårlig feedback"); break;
    default: Serial.println("Ukendt knap"); break;
  }
}

// === Gå i dyb søvn og vågn op på næste knaptryk ===
void goToDeepSleep() {
  Serial.println("💤 Går i deep sleep...");
  esp_sleep_enable_ext1_wakeup(
    (1ULL << buttonPins[0]) | (1ULL << buttonPins[1]) |
    (1ULL << buttonPins[2]) | (1ULL << buttonPins[3]),
    ESP_EXT1_WAKEUP_ALL_LOW // Vågn hvis en hvilken som helst knap er trykket
  );
  delay(500); // Giv systemet lidt tid
  esp_deep_sleep_start(); // Gå i deep sleep
}
