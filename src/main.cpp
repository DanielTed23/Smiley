#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "esp_sleep.h"

// === Wi-Fi credentials ===
const char* ssid = "IoT_H3/4";
const char* password = "98806829";

// === GPIO pin config ===
const int buttonPins[] = {27, 26, 25, 33};
const int ledPins[]    = {5, 18, 19, 21};
const int numButtons = 4;

// === Timing ===
const unsigned long ledDuration = 7000;
const unsigned long stayAwakeDuration = 10 * 60 * 1000; // 10 min

RTC_DATA_ATTR bool activePeriod = false;
RTC_DATA_ATTR unsigned long bootTime = 0;

int lastButton = -1;
unsigned long awakeStart = 0;

void connectWiFiAndPrintTime();
void goToDeepSleep();

void setup() {
  Serial.begin(115200);
  delay(200);

  for (int i = 0; i < numButtons; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  awakeStart = millis();

  if (cause == ESP_SLEEP_WAKEUP_EXT1) {
    // Woken by button
    for (int i = 0; i < numButtons; i++) {
      if (digitalRead(buttonPins[i]) == LOW) {
        delay(20); // debounce
        if (digitalRead(buttonPins[i]) == LOW) {
          lastButton = i;
          activePeriod = true;
          Serial.printf("Knap %d trykket – LED tændes\n", i + 1);
          digitalWrite(ledPins[i], HIGH);
          connectWiFiAndPrintTime();
          delay(ledDuration);
          digitalWrite(ledPins[i], LOW);
          break;
        }
      }
    }
  } else {
    Serial.println("Første opstart eller timer – tjekker knapper...");

    for (int i = 0; i < numButtons; i++) {
      if (digitalRead(buttonPins[i]) == LOW) {
        delay(20);
        if (digitalRead(buttonPins[i]) == LOW) {
          lastButton = i;
          activePeriod = true;
          Serial.printf(" Knap %d blev trykket – starter aktiv periode\n", i + 1);
          digitalWrite(ledPins[i], HIGH);
          connectWiFiAndPrintTime();
          delay(ledDuration);
          digitalWrite(ledPins[i], LOW);
          break;
        }
      }
    }
  }
}

void loop() {
  if (!activePeriod) {
    goToDeepSleep(); // ikke aktiv periode – gå i dvale
  }

  if (millis() - awakeStart >= stayAwakeDuration) {
    Serial.println(" Aktiv periode er slut – går i dvale");
    activePeriod = false;
    goToDeepSleep();
  }

  // Reager på andre knapper under aktiv periode
  for (int i = 0; i < numButtons; i++) {
    if (digitalRead(buttonPins[i]) == LOW) {
      delay(20);
      if (digitalRead(buttonPins[i]) == LOW) {
        Serial.printf("🔁 Ny knap %d trykket – tænd LED og vis tid\n", i + 1);
        digitalWrite(ledPins[i], HIGH);
        connectWiFiAndPrintTime();
        delay(ledDuration);
        digitalWrite(ledPins[i], LOW);
      }
    }
  }
}

void goToDeepSleep() {
  Serial.println("💤 Går i deep sleep...");

  esp_sleep_enable_ext1_wakeup(
    (1ULL << buttonPins[0]) | (1ULL << buttonPins[1]) |
    (1ULL << buttonPins[2]) | (1ULL << buttonPins[3]),
    ESP_EXT1_WAKEUP_ALL_LOW
  );

  delay(1000);
  esp_deep_sleep_start();
}

void connectWiFiAndPrintTime() {
  WiFi.begin(ssid, password);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    configTzTime("CET-1CEST,M3.5.0/02,M10.5.0/03", "pool.ntp.org");
    while (time(nullptr) < 100000) delay(500);

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char buf[64];
      strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
      Serial.printf("Tid: %s\n", buf);
    }
  } else {
    Serial.println("Wi-Fi kunne ikke tilsluttes.");
  }
}
