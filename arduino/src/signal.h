#pragma once

struct SignalConfig
{
    // Base signal
    float a1, f1; // 2*sin(2π*3*t)
    float a2, f2; // 4*sin(2π*5*t)

    // Noise
    float noiseStdDev; // σ for Gaussian noise n(t)

    // Anomaly injection
    float anomalyProb; // p per sample
    float anomalyMin; // U(5,15) lower bound
    float anomalyMax; // U(5,15) upper bound
};

// Clean signal (as before)
float generateSignal(float t);

// Noisy signal with anomaly injection
// anomalyOut: if not nullptr, set to 1 if this sample is an anomaly, 0 otherwise
float generateNoisySignal(float t, const SignalConfig& config, int* anomalyOut);

struct SignalDef
{
    const char* name;
    float components[4][2]; // up to 4 [amplitude, frequency] pairs
    int numComponents;
};

float generateSignalFromDef(float t, const SignalDef& def);
