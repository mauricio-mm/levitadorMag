
#include <Arduino.h>
#include "esp32-hal-cpu.h"
#include "altura.h"

// Sensores Hall
#define SENSOR1_PIN 32
#define SENSOR2_PIN 33

// Ponte H L298N
#define MOTOR_IN1 22
#define MOTOR_IN2 23
#define ENA 25

// Controlador PD
const float SETPOINT_CM = 2.00f;
const float KP = 60.0f;
const float KD = 0.0f;

const int PWM_BASE = 170;
const int PWM_MIN = 160;
const int PWM_MAX = 180;

// Amostragem e segurança
const unsigned long TS_MS = 20;
const unsigned long MAX_ATIVO_MS = 15000;

const float ALPHA_MEDIA = 0.25f;
const float ALPHA_VELOCIDADE = 0.10f;

enum Estado { DESLIGADO, AGUARDANDO, CONTROLE_PD };
Estado estado = DESLIGADO;

// Variáveis de controle
float mediaFiltrada = 0.0f;
float alturaAnterior = NAN;
float velocidadeFiltrada = 0.0f;

bool filtroIniciado = false;

int pwmAtual = 0;
int contadorEstavel = 0;
int contadorFora = 0;

unsigned long ultimoControle = 0;
unsigned long ultimaImpressao = 0;
unsigned long iniciouAtivo = 0;

// Desliga o eletroímã
void desligar(const char* motivo) {
    analogWrite(ENA, 0);

    pwmAtual = 0;
    estado = DESLIGADO;
    contadorEstavel = 0;
    contadorFora = 0;

    alturaAnterior = NAN;
    velocidadeFiltrada = 0.0f;

    Serial.printf("DESLIGADO: %s\n", motivo);
}

// Atualiza o PWM somente quando necessário
void aplicarPWM(int valor) {
    valor = constrain(valor, 0, PWM_MAX);

    if (valor != pwmAtual) {
        pwmAtual = valor;
        analogWrite(ENA, pwmAtual);
    }
}

void setup() {
    Serial.begin(115200);
    setCpuFrequencyMhz(240);

    Serial.printf("CPU: %u MHz\n", getCpuFrequencyMhz());

    // Configuração dos sensores
    analogReadResolution(12);
    analogSetPinAttenuation(SENSOR1_PIN, ADC_11db);
    analogSetPinAttenuation(SENSOR2_PIN, ADC_11db);

    pinMode(SENSOR1_PIN, INPUT);
    pinMode(SENSOR2_PIN, INPUT);

    // Configuração da ponte H
    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(ENA, OUTPUT);

    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    analogWrite(ENA, 0);

    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);

    Serial.println("\n=== CONTROLE PD ===");
    Serial.printf("Setpoint: %.2f cm\n", SETPOINT_CM);
    Serial.printf("Kp: %.2f | Kd: %.2f\n", KP, KD);
    Serial.printf("PWM Base: %d | Min: %d | Max: %d\n", PWM_BASE, PWM_MIN, PWM_MAX);

    Serial.println("\nComandos:");
    Serial.println("p = preparar controle PD");
    Serial.println("x = desligar eletroima");
    Serial.println("Limite: 5 segundos por ativacao\n");
}

void loop() {
    // Comandos via Monitor Serial
    while (Serial.available()) {
        char comando = (char)Serial.read();

        if (comando == 'x' || comando == 'X') {
            desligar("comando x");
        }

        if ((comando == 'p' || comando == 'P') && estado == DESLIGADO) {
            estado = AGUARDANDO;
            contadorEstavel = 0;
            contadorFora = 0;

            alturaAnterior = NAN;
            velocidadeFiltrada = 0.0f;

            Serial.println("AGUARDANDO: posicione a esfera proxima de 2.00 cm");
            Serial.println("PWM permanece em 0 ate a ativacao.");
        }
    }

    // Controle de tempo com dt real
    unsigned long agora = millis();
    unsigned long tempoDecorrido = agora - ultimoControle;

    if (tempoDecorrido < TS_MS) return;

    float dt = tempoDecorrido / 1000.0f;
    ultimoControle = agora;

    // Leitura dos sensores
    int s1 = analogRead(SENSOR1_PIN);
    int s2 = analogRead(SENSOR2_PIN);

    float media = (s1 + s2) / 2.0f;

    // Filtro exponencial da média ADC
    if (!filtroIniciado) {
        mediaFiltrada = media;
        filtroIniciado = true;
    } else {
        mediaFiltrada += ALPHA_MEDIA * (media - mediaFiltrada);
    }

    // Estimativa de altura
    float altura = calcularAltura(mediaFiltrada);

    // Validação dos sensores
    bool sensoresValidos =
        s1 > 30 &&
        s2 > 30 &&
        s1 < 4050 &&
        s2 < 4050 &&
        abs(s1 - s2) < 700;

    // Aguardando a esfera estar próxima do setpoint
    if (estado == AGUARDANDO) {
        if (sensoresValidos && altura >= 1.80f && altura <= 2.20f) {
            contadorEstavel++;
        } else {
            contadorEstavel = 0;
        }

        if (contadorEstavel >= 20) {
            contadorFora = 0;
            iniciouAtivo = agora;

            // Reinicia a estimativa de velocidade
            alturaAnterior = altura;
            velocidadeFiltrada = 0.0f;

            aplicarPWM(PWM_BASE);
            estado = CONTROLE_PD;

            Serial.println("\nPD ATIVO: PWM inicial 170");
        }
    }

    // Controlador PD
    else if (estado == CONTROLE_PD) {
        if (agora - iniciouAtivo >= MAX_ATIVO_MS) {
            desligar("limite de 5 segundos");
        }
        else if (!sensoresValidos) {
            desligar("leitura invalida/saturada ou assimetria excessiva");
        }
        else {
            // Segurança: altura fora da região de controle
            if (altura < 1.60f || altura > 2.40f) {
                contadorFora++;
            } else {
                contadorFora = 0;
            }

            if (contadorFora >= 3) {
                desligar("altura estimada fora da janela");
            }
            else {
                // Derivada da altura usando dt real
                if (isnan(alturaAnterior)) alturaAnterior = altura;

                float velocidade = (altura - alturaAnterior) / dt;
                velocidadeFiltrada += ALPHA_VELOCIDADE * (velocidade - velocidadeFiltrada);
                alturaAnterior = altura;

                // Erro proporcional
                float erro = SETPOINT_CM - altura;

                // Contribuições P e D
                float proporcional = KP * erro;
                float derivativo = -KD * velocidadeFiltrada;

                // Controlador PD
                float controle = PWM_BASE + proporcional + derivativo;

                int alvo = constrain((int)roundf(controle), PWM_MIN, PWM_MAX);

                // Limita a variação a 1 PWM por ciclo
                if (alvo > pwmAtual + 1) alvo = pwmAtual + 1;
                if (alvo < pwmAtual - 1) alvo = pwmAtual - 1;

                aplicarPWM(alvo);
            }
        }
    }

    // Impressão a cada 100 ms
    if (agora - ultimaImpressao >= 100) {
        ultimaImpressao = agora;

        const char* nome = estado == CONTROLE_PD ? "PD" :
                           estado == AGUARDANDO ? "ESPERA" : "OFF";

        float erro = SETPOINT_CM - altura;
        float proporcional = KP * erro;
        float derivativo = -KD * velocidadeFiltrada;

        Serial.printf(
            "%s | S1=%d S2=%d | ADC=%.1f | h=%.3f cm | erro=%+.3f | P=%+.2f | D=%+.2f | vel=%+.3f cm/s | PWM=%d\n",
            nome, s1, s2, mediaFiltrada, altura, erro,
            proporcional, derivativo, velocidadeFiltrada, pwmAtual
        );
    }
}