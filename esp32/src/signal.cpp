#include "signal.hpp"
#include <cmath>
#include "esp_random.h"

#define TWO_PI_F (2.0f * M_PI)


static float esp_random_float() {
    return (float) esp_random() / (float) UINT32_MAX;
}

// ======= SIGNAL DEFINITIONS =======

const SignalDef SIGNAL_1 = {
    .name = "2sin(2pi*3t)+4sin(2pi*5t)",
    .components = {{2.0f, 3.0f}, {4.0f, 5.0f}},
    .numComponents = 2
};

const SignalDef SIGNAL_2 = {
    .name = "4sin(2pi*2t)",
    .components = {{4.0f, 2.0f}},
    .numComponents = 1
};

const SignalDef SIGNAL_3 = {
    .name = "2sin(2pi*10t)+3sin(2pi*45t)",
    .components = {{2.0f, 10.0f}, {3.0f, 45.0f}},
    .numComponents = 2
};

const SignalDef *ALL_SIGNALS[3] = {&SIGNAL_1, &SIGNAL_2, &SIGNAL_3};

const SignalConfig DEFAULT_NOISY_CONFIG = {
    .a1 = 2.0f, .f1 = 3.0f,
    .a2 = 4.0f, .f2 = 5.0f,
    .noiseStdDev = 0.2f,
    .anomalyProb = 0.02f,
    .anomalyMin = 5.0f,
    .anomalyMax = 15.0f
};

// ======= SIGNAL GENERATION =======

float generateSignal(float t, const SignalDef &def) {
    float s = 0.0f;
    for (int i = 0; i < def.numComponents; i++)
        s += def.components[i][0] * sinf(TWO_PI_F * def.components[i][1] * t);
    return s;
}

// Box-Muller transform for Gaussian noise
static float gaussianNoise(float stdDev) {
    float u1 = esp_random_float() * (1.0f - 1e-7f) + 1e-7f; // avoid log(0)
    float u2 = esp_random_float();
    float z = sqrtf(-2.0f * logf(u1)) * cosf(TWO_PI_F * u2);
    return z * stdDev;
}

float generateNoisySignal(float t, const SignalConfig &config, int *anomalyOut) {
    float s = config.a1 * sinf(TWO_PI_F * config.f1 * t) +
              config.a2 * sinf(TWO_PI_F * config.f2 * t);

    s += gaussianNoise(config.noiseStdDev);

    float r = esp_random_float();
    if (r < config.anomalyProb) {
        float magnitude = config.anomalyMin + esp_random_float() * (config.anomalyMax - config.anomalyMin);
        if (esp_random() % 2) magnitude = -magnitude;
        s += magnitude;
        if (anomalyOut) *anomalyOut = 1;
    } else {
        if (anomalyOut) *anomalyOut = 0;
    }

    return s;
}
