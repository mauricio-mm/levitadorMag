#include <Arduino.h>
#include "esp32-hal-cpu.h"

// Sensores
#define SENSOR1_PIN 32
#define SENSOR2_PIN 33

// Ponte H L298N
#define MOTOR_IN1 22
#define MOTOR_IN2 23

// Frequência do controle
#define CONTROL_FREQ_HZ 1000

// Período de amostragem em microssegundos
#define SAMPLE_TIME_US (1000000UL / CONTROL_FREQ_HZ)

// Ts em segundos para futuros cálculos do controlador
const float Ts = 1.0f / CONTROL_FREQ_HZ;

uint32_t ultimoTempo = 0;

int count = 0;

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

    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);

    ultimoTempo = micros();
}

void loop() {

    uint32_t agora = micros();

    if ((uint32_t)(agora - ultimoTempo) >= SAMPLE_TIME_US) {

        ultimoTempo += SAMPLE_TIME_US;

        int sensor1 = analogRead(SENSOR1_PIN);
        int sensor2 = analogRead(SENSOR2_PIN);

        if (count < 100) {
            digitalWrite(MOTOR_IN1, LOW);
            digitalWrite(MOTOR_IN2, LOW);
        }
        else if (count < 200) {
            digitalWrite(MOTOR_IN1, HIGH);
            digitalWrite(MOTOR_IN2, LOW);
        }
        else if (count < 300) {
            digitalWrite(MOTOR_IN1, LOW);
            digitalWrite(MOTOR_IN2, LOW);
        }
        else if (count < 400) {
            digitalWrite(MOTOR_IN1, LOW);
            digitalWrite(MOTOR_IN2, HIGH);
        }
        else {
            count = 0;
            digitalWrite(MOTOR_IN1, LOW);
            digitalWrite(MOTOR_IN2, LOW);
        }

        if (count % 20 == 0) {

            Serial.printf("S1: %d | S2: %d | count: %d | Ts: %.6f\n", sensor1, sensor2, count, Ts);
        }

        count++;
    }
}