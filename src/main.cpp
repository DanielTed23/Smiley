#include <Arduino.h>
#include <WiFi.h>      // Til Wi-Fi
#include <time.h>      // Til NTP-tid

// === Wi-Fi credentials ===
const char* ssid = "IoT_H3/4";        
const char* password = "98806829";    

// === Hardware opsætning ===
const int buttonPins[] = {27, 26, 25, 33};   // GPIO for knapper
const int ledPins[]    = {5, 18, 19, 21};    // GPIO for LED'er
const int numButtons = 4;

bool buttonPressed = false;
unsigned long lastPressTime = 0;
const unsigned long lockoutDuration = 7000;  // 7 sekunder lås efter tryk

// === Fremad-deklaration ===
void printCurrentTime();  // <- Korrekt placering: udenfor setup()

void setup() {
  Serial.begin(115200);
  delay(1000);

  // === Setup GPIO ===
  for (int i = 0; i < numButtons; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);   // Knap til GND
    pinMode(ledPins[i], OUTPUT);            // LED output
    digitalWrite(ledPins[i], LOW);          // LED slukket ved start
  }

  // === Tilslut Wi-Fi ===
  Serial.println("Tilslutter Wi-Fi...");
  WiFi.begin(ssid, password);

  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
    delay(500);
    Serial.print(".");
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" Wi-Fi forbundet!");
    Serial.print("IP-adresse: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" Wi-Fi kunne ikke tilsluttes.");
    return;
  }

  // === Synkronisér tid med NTP-server og dansk tidszone ===
  configTzTime("CET-1CEST,M3.5.0/02,M10.5.0/03", "pool.ntp.org", "time.nist.gov");

  Serial.print(" Venter på tidssynkronisering");
  while (time(nullptr) < 100000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n⏰ Tid synkroniseret!");
}

void loop() {
  if (!buttonPressed) {
    for (int i = 0; i < numButtons; i++) {
      if (digitalRead(buttonPins[i]) == LOW) {
        delay(20); // debounce
        if (digitalRead(buttonPins[i]) == LOW) {
          digitalWrite(ledPins[i], HIGH);
          lastPressTime = millis();
          buttonPressed = true;

          Serial.printf("Knap %d trykket – LED tændes i 7 sekunder\n", i + 1);
          printCurrentTime();
          break;
        }
      }
    }
  } else {
    if (millis() - lastPressTime >= lockoutDuration) {
      for (int i = 0; i < numButtons; i++) {
        digitalWrite(ledPins[i], LOW);
      }
      buttonPressed = false;
      Serial.println("System klar til ny feedback");
    }
  }
}

void printCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println(" Kunne ikke hente tid.");
    return;
  }
  char timeStr[64];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
  Serial.print(" Aktuel tid: ");
  Serial.println(timeStr);
}
