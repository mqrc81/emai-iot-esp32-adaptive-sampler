#pragma once
#include <Arduino.h>

struct SignalDef {
    const char *name;
    float components[4][2]; // [amplitude, frequency] pairs
    int numComponents;
};

struct SignalConfig {
    float a1, f1;
    float a2, f2;
    float noiseStdDev;
    float anomalyProb;
    float anomalyMin;
    float anomalyMax;
};

// Predefined signals for experiment
extern const SignalDef SIGNAL_1; // 2sin(2π·3t) + 4sin(2π·5t) — baseline
extern const SignalDef SIGNAL_2; // 4sin(2π·2t)               — low frequency
extern const SignalDef SIGNAL_3; // 2sin(2π·10t) + 3sin(2π·45t) — high frequency
extern const SignalDef *ALL_SIGNALS[3];

// Default noisy signal config
extern const SignalConfig DEFAULT_NOISY_CONFIG;

float generateSignal(float t, const SignalDef &def);

float generateNoisySignal(float t, const SignalConfig &config, int *anomalyOut);
