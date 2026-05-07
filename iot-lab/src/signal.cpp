#include "signal.hpp"
#include <Arduino.h>
#include <stdlib.h>

#define TWO_PI_F (2.0f * M_PI)

float generateSignal(float t) {
    return 2.0f * sinf(TWO_PI_F * 3.0f * t) +
           4.0f * sinf(TWO_PI_F * 5.0f * t);
}

// Box-Muller transform for Gaussian noise
static float gaussianNoise(float stdDev) {
    float u1 = (float) (rand() + 1) / ((float) RAND_MAX + 1);
    float u2 = (float) rand() / (float) RAND_MAX;
    float z = sqrtf(-2.0f * logf(u1)) * cosf(TWO_PI_F * u2);
    return z * stdDev;
}

float generateNoisySignal(float t, const struct SignalConfig &config, int *anomalyOut) {
    // Base signal
    float s = config.a1 * sinf(TWO_PI_F * config.f1 * t) +
              config.a2 * sinf(TWO_PI_F * config.f2 * t);

    // Gaussian noise n(t)
    s += gaussianNoise(config.noiseStdDev);

    // Anomaly injection A(t)
    float r = (float) rand() / (float) RAND_MAX;
    if (r < config.anomalyProb) {
        float magnitude = config.anomalyMin +
                          (float) rand() / (float) RAND_MAX * (config.anomalyMax - config.anomalyMin);
        // Random sign
        if (rand() % 2) magnitude = -magnitude;
        s += magnitude;
        if (anomalyOut) *anomalyOut = 1;
    } else {
        if (anomalyOut) *anomalyOut = 0;
    }

    return s;
}

float generateSignalFromDef(float t, const struct SignalDef &def) {
    float s = 0.0f;
    for (int i = 0; i < def.numComponents; i++)
        s += def.components[i][0] * sinf(TWO_PI_F * def.components[i][1] * t);
    return s;
}
