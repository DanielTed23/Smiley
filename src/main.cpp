// men vi inkluderer Arduino.h eksplicit for klarhed på ESP32
#include <Arduino.h>

const int buttonPins[] = {27,26,25,33};   // GPIO for knapper
const int ledPins[] = {5, 18, 19, 21};      // GPIO for LED'er
const int numButtons = 4;

bool buttonPressed = false;
unsigned long lastPressTime = 0;
const unsigned long lockoutDuration = 7000;  // 7 sekunder lås efter tryk

void setup() {
  Serial.begin(115200);  // Debug over USB

  for (int i = 0; i < numButtons; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);   // Knap til GND
    pinMode(ledPins[i], OUTPUT);            // LED output
    digitalWrite(ledPins[i], LOW);          // LED slukket ved start
  }
}

void loop() {
  if (!buttonPressed) {
    for (int i = 0; i < numButtons; i++) {
      if (digitalRead(buttonPins[i]) == LOW) {  // Knap trykket (lav signal)
        delay(20); // simpel debounce
        if (digitalRead(buttonPins[i]) == LOW) {
          // Tænd LED og start lock
          digitalWrite(ledPins[i], HIGH);
          lastPressTime = millis();
          buttonPressed = true;
          Serial.printf("Knap %d trykket – LED tændes i 7 sekunder\n", i + 1);
          break;
        }
      }
    }
  } else {
    // Under lockout: LED tændt, ignorer knapper
    if (millis() - lastPressTime >= lockoutDuration) {
      for (int i = 0; i < numButtons; i++) {
        digitalWrite(ledPins[i], LOW);  // Sluk LED'er
      }
      buttonPressed = false;
      Serial.println("System klar til ny feedback");
    }
  }
}