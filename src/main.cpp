#include <Arduino.h>
#include "esp32-hal-cpu.h"

// Sensores
#define SENSOR1_PIN 32
#define SENSOR2_PIN 33

// Ponte H L298N
#define MOTOR_IN1 22
#define MOTOR_IN2 23
#define ENA 25

int f = 128; 

void setup() {
    Serial.begin(115200);
    setCpuFrequencyMhz(240);

    Serial.printf("CPU: %u MHz\n", getCpuFrequencyMhz());
    
    analogReadResolution(12);

    analogSetPinAttenuation(SENSOR1_PIN, ADC_11db);
    analogSetPinAttenuation(SENSOR2_PIN, ADC_11db);

    pinMode(SENSOR1_PIN, INPUT);
    pinMode(SENSOR2_PIN, INPUT);
    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(ENA, OUTPUT);

    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
}

void loop() {
    
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim(); 
        if (input.length() > 0) {
            int valor = input.toInt();
            f = constrain(valor, 0, 255); 
            Serial.printf("Nova velocidade definida: %d\n", f);
        }
    }

    int sensor1 = analogRead(SENSOR1_PIN);
    int sensor2 = analogRead(SENSOR2_PIN);
    
    analogWrite(ENA, f);
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
    
    Serial.printf("S1: %d | S2: %d | PWM: %d\n", sensor1, sensor2, f);

    delay(50); 
}
