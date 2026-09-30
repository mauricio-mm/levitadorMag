#include <Arduino.h>

#define SENSOR1_PIN 34
#define SENSOR2_PIN 35

void setup() {
    Serial.begin(115200);

    analogReadResolution(12);

    pinMode(SENSOR1_PIN, INPUT);
    pinMode(SENSOR2_PIN, INPUT);
}

void loop() {
    int sensor1 = analogRead(SENSOR1_PIN);
    int sensor2 = analogRead(SENSOR2_PIN);

    Serial.printf("S1: %d | S2: %d\n", sensor1, sensor2);

    delay(100);
}