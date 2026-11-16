

#include "altura.h"

const int TOTAL_PONTOS = 13;

// Nova curva - Ensaio 2 - PWM = 0
const float adcCalibrado[TOTAL_PONTOS] = {
    1459.8103, 1774.1713, 2047.2790, 2099.1617,
    2339.9337, 2455.1870, 2641.1987, 2777.2413,
    2805.7127, 2847.3843, 2894.1857, 2916.0130,
    2931.8737
};

const float alturaCalibrada[TOTAL_PONTOS] = {
    0.50, 0.75, 1.00, 1.25,
    1.50, 1.75, 2.00, 2.25,
    2.50, 2.75, 3.00, 3.25,
    3.50
};

float calcularAltura(float media) {
    if (media <= adcCalibrado[0]) return alturaCalibrada[0];

    for (int i = 1; i < TOTAL_PONTOS; i++) {
        if (media <= adcCalibrado[i]) {
            float fator = (media - adcCalibrado[i - 1]) / (adcCalibrado[i] - adcCalibrado[i - 1]);

            return alturaCalibrada[i - 1] + fator * (alturaCalibrada[i] - alturaCalibrada[i - 1]);
        }
    }

    return alturaCalibrada[TOTAL_PONTOS - 1];
}
