const int TOTAL_PONTOS = 11;

const float adcCalibrado[TOTAL_PONTOS] = {
    1485.494,
    1708.143,
    2055.214,
    2249.304,
    2523.126,
    2630.439,
    2744.676,
    206.717,
    2842.500,
    2869.774,
    2908.237
};

const float alturaCalibrada[TOTAL_PONTOS] = {
    1.00,
    1.25,
    1.50,
    1.75,
    2.00,
    2.25,
    2.50,
    2.75,
    3.00,
    3.25,
    3.60};

float calcularAltura(float media) {
    if (media <= adcCalibrado[0]) {
        return alturaCalibrada[0];
    }

    if (media >= adcCalibrado[TOTAL_PONTOS - 1]) {
        return alturaCalibrada[TOTAL_PONTOS - 1];
    }

    for (int i = 0; i < TOTAL_PONTOS - 1; i++) {
        if (media >= adcCalibrado[i] && media <= adcCalibrado[i + 1]) {
            float proporcao =
                (media - adcCalibrado[i]) /
                (adcCalibrado[i + 1] - adcCalibrado[i]);

            return alturaCalibrada[i] +
                proporcao * (alturaCalibrada[i + 1] - alturaCalibrada[i]);
        }
    }

    return -1.0;
}