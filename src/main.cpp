
#include <Arduino.h>
#include "esp32-hal-cpu.h"
#include "altura.h"
#include <math.h>
#include <stdio.h>

#define SENSOR1_PIN 32
#define SENSOR2_PIN 33

#define MOTOR_IN1 22
#define MOTOR_IN2 23
#define ENA 25

float ALTURA_REFERENCIA = 2.0f;
float PWM_BASE = 190.0f;
float KP = 50.0f;
float KD = 4.5f;
float polo = 0.8;

const float KP_MIN = 0.0f;
const float KP_MAX = 100.0f;
const float KD_MIN = 0.0f;
const float KD_MAX = 20.0f;

const float ALTURA_MIN = 1.0f;
const float ALTURA_MAX = 3.4f;

const float PWM_BASE_MIN = 0.0f;
const float PWM_BASE_MAX = 230.0f;

const int PWM_MIN = 0;
const int PWM_MAX = 245;

const unsigned long INTERVALO_CONTROLE = 1000;
const unsigned long INTERVALO_PRINT = 25000;

float velocidade = 0.0f;
float alturaAnteriorVelocidade = 0.0f;

unsigned long ultimoCalculoVelocidade = 0;
const unsigned long INTERVALO_VELOCIDADE = 10000;

float alturaAtual = 0;
float alturaFiltrada = 0.0f;
float alturaFiltradaAnterior = 0.0f;
float pwmCalculado = 0.0f;

bool controleAtivo = false;
bool primeiraLeitura = true;
bool aguardandoGanhos = false;
bool aguardandoAltura = false;
bool aguardandoPWM = false;

unsigned long ultimoControle = 0;
unsigned long ultimoPrint = 0;

void desligarMotor() {
    analogWrite(ENA, 0);
    pwmCalculado = 0;
}

void iniciarControle() {
    int sensor1 = analogRead(SENSOR1_PIN);
    int sensor2 = analogRead(SENSOR2_PIN);

    float media = (sensor1 + sensor2) / 2.0f;
    alturaAtual = calcularAltura(media);

    primeiraLeitura = false;
    ultimoControle = micros();
    ultimoPrint = millis() * 1000UL;

    alturaFiltrada = alturaAtual;
    alturaFiltradaAnterior = alturaAtual;
    alturaAnteriorVelocidade = alturaAtual;

    velocidade = 0.0f;
    ultimoCalculoVelocidade = micros();

    controleAtivo = true;

    Serial.println("Controle iniciado");
}

void processarComando(String comando) {
    comando.trim();

    if (comando.length() == 0) {
        return;
    }

    if (aguardandoGanhos || aguardandoAltura || aguardandoPWM) {
        float valor1;
        float valor2;
        float valor3;

        if (aguardandoGanhos) {
            if (sscanf(comando.c_str(), "%f,%f,%f", &valor1, &valor2, &valor3) == 3 &&
                isfinite(valor1) && isfinite(valor2) && isfinite(valor3)) {

                if (valor1 >= KP_MIN && valor1 <= KP_MAX &&
                    valor2 >= KD_MIN && valor2 <= KD_MAX &&
                    valor3 >= 0 && valor3 <= 100) {

                    KP = valor1;
                    KD = valor2;
                    polo = valor3;

                    Serial.printf("Novos ganhos: KP = %.2f | KD = %.2f\n  | polo = %.2f\n", KP, KD, polo);
                } else {
                    Serial.println("Valores fora dos limites permitidos");
                }
            } else {
                Serial.println("Formato invalido. Utilize: KP,KD");
            }

            aguardandoGanhos = false;
            return;
        }

        if (aguardandoAltura) {
            float alturaNova = comando.toFloat();

            if (isfinite(alturaNova) && alturaNova >= ALTURA_MIN &&
                alturaNova <= ALTURA_MAX) {

                ALTURA_REFERENCIA = alturaNova;
                Serial.printf("Nova altura de referencia: %.2f cm\n", ALTURA_REFERENCIA);
            } else {
                Serial.printf("Altura invalida. Utilize um valor entre %.1f e %.1f cm\n",
                              ALTURA_MIN, ALTURA_MAX);
            }

            aguardandoAltura = false;
            return;
        }

        if (aguardandoPWM) {
            float pwmNovo = comando.toFloat();

            if (isfinite(pwmNovo) && pwmNovo >= PWM_BASE_MIN &&
                pwmNovo <= PWM_BASE_MAX) {

                PWM_BASE = pwmNovo;
                Serial.printf("Novo PWM base: %.2f\n", PWM_BASE);
            } else {
                Serial.printf("PWM invalido. Utilize um valor entre %.0f e %.0f\n",
                              PWM_BASE_MIN, PWM_BASE_MAX);
            }

            aguardandoPWM = false;
            return;
        }
    }

    comando.toLowerCase();

    if (comando == "s") {
        if (!controleAtivo) {
            iniciarControle();
        } else {
            Serial.println("Controle ja esta ativo");
        }
    }
    else if (comando == "p") {
        controleAtivo = false;
        desligarMotor();
        Serial.println("Controle pausado. PWM desligado");
    }
    else if (comando == "c") {
        aguardandoGanhos = true;
        Serial.println("Digite KP,KD. Exemplo: 30,5");
    }
    else if (comando == "h") {
        aguardandoAltura = true;
        Serial.printf("Digite a nova altura de referencia em cm (%.1f a %.1f):\n",
                      ALTURA_MIN, ALTURA_MAX);
    }
    else if (comando == "j") {
        aguardandoPWM = true;
        Serial.printf("Digite o novo PWM base (%.0f a %.0f):\n",
                      PWM_BASE_MIN, PWM_BASE_MAX);
    }
    else {
        Serial.println("Comando desconhecido");
        Serial.println("Comandos: s, p, c, h, j");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(50);

    setCpuFrequencyMhz(240);

    analogReadResolution(12);

    analogSetPinAttenuation(SENSOR1_PIN, ADC_11db);
    analogSetPinAttenuation(SENSOR2_PIN, ADC_11db);

    pinMode(SENSOR1_PIN, INPUT);
    pinMode(SENSOR2_PIN, INPUT);

    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(ENA, OUTPUT);

    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);

    desligarMotor();

    Serial.println("Levitador magnetico pronto");
    Serial.println("Envie s para iniciar");
    Serial.println("Envie p para pausar");
    Serial.println("Envie c para configurar KP e KD");
    Serial.println("Envie h para configurar a altura de referencia");
    Serial.println("Envie j para configurar o PWM base");

    Serial.printf("Altura: %.2f cm | PWM Base: %.2f\n",
                  ALTURA_REFERENCIA, PWM_BASE);
    Serial.printf("KP: %.2f | KD: %.2f\n", KP, KD);
}

void loop() {
    if (Serial.available()) {
        String comando = Serial.readStringUntil('\n');
        processarComando(comando);
    }

    if (!controleAtivo) {
        return;
    }

    unsigned long agora = micros();

    if (agora - ultimoControle >= INTERVALO_CONTROLE) {
        float dt = (agora - ultimoControle) / 1000000.0f;
        ultimoControle = agora;                 

        int sensor1 = analogRead(SENSOR1_PIN);
        int sensor2 = analogRead(SENSOR2_PIN);

        float media = (sensor1 + sensor2) / 2.0f;
        float alturaAtual = calcularAltura(media);

        // if (alturaAtual > ALTURA_REFERENCIA) {
        //     alturaAtual = ALTURA_REFERENCIA + (alturaAtual - ALTURA_REFERENCIA)/2;
        // }

        if (primeiraLeitura) {
            alturaFiltrada = alturaAtual;
            alturaFiltradaAnterior = alturaAtual;
            primeiraLeitura = false;
        }

        alturaFiltrada = (polo*alturaFiltradaAnterior) + (( 1 - polo) * alturaAtual);

        if (agora - ultimoCalculoVelocidade >= INTERVALO_VELOCIDADE) {
            float dtVelocidade = (agora - ultimoCalculoVelocidade) / 1000000.0f;

            velocidade = (alturaFiltrada - alturaAnteriorVelocidade) / dtVelocidade;

            alturaAnteriorVelocidade = alturaFiltrada;
            ultimoCalculoVelocidade = agora;
        }        

        alturaFiltradaAnterior = alturaFiltrada;

        float erro = ALTURA_REFERENCIA - alturaFiltrada;
        
        // if (erro < 0)
        // {
        //     erro = erro/2;
        // }

        pwmCalculado = PWM_BASE + KP * erro - KD * velocidade;

        pwmCalculado = constrain(pwmCalculado, PWM_MIN, PWM_MAX);

        if (alturaFiltrada >= 3.45f) {
            pwmCalculado = 0;
        }

        if (alturaFiltrada <= 1.40f) {
            pwmCalculado = PWM_MAX;
        }

        analogWrite(ENA, (int)pwmCalculado);

        if (agora - ultimoPrint >= INTERVALO_PRINT) {
            ultimoPrint = agora;

            Serial.printf(
                "Tempo: %0.3f | Altura Filtrada: %.3f | Ref: %.2f | Erro: %.3f | Vel: %.3f | PWM: %.0f | Base: %.0f | KP: %.2f | KD: %.2f\n",
                agora / 1000000.0,                
                alturaFiltrada,
                ALTURA_REFERENCIA,
                erro,
                velocidade,
                pwmCalculado,
                PWM_BASE,
                KP,
                KD
            );
        }
    }
}
