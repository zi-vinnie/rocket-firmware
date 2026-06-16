//
// Created by josiah on 15/06/2026.
//

#include <Arduino.h>
#include <cmath>
#include "lights.h"

namespace {
    // Displays a white light for a given duration
    void whiteLight(int durationMs) {
        digitalWrite(LED_RED, LOW);
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_BLUE, LOW);
        delay(durationMs);
        digitalWrite(LED_RED, HIGH);
        digitalWrite(LED_GREEN, HIGH);
        digitalWrite(LED_BLUE, HIGH);
    }
} // namespace

void setup_rgb_lights() {
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);
}

void display_apogee(const float apogee, const int kPrecision) {
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);

    const auto apogeeString = String(apogee, kPrecision);

    whiteLight(1000);
    int currentLED;
    for (unsigned int i = 0; i < apogeeString.length(); i++) {
        delay(2000);
        const char c = apogeeString.charAt(i);
        if (c == '.') {
            whiteLight(850);
            continue;
        }
        if (c < '0' || c > '9') {
            continue;
        }

        int digit = c - '0';
        if (digit == 0) {
            digit = 10;
        }

        const int remainder = i % 3;
        if (remainder == 0) {
            currentLED = LED_RED;
        } else if (remainder == 1) {
            currentLED = LED_GREEN;
        } else {
            currentLED = LED_BLUE;
        }

        for (int j = 0; j < digit; j++) {
            digitalWrite(currentLED, LOW);
            delay(150);
            digitalWrite(currentLED, HIGH);
            delay(700);
        }
    }
    delay(2000);
    whiteLight(1000);
}
