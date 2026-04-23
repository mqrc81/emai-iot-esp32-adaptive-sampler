#pragma once

struct SignalConfig
{
    float a1, f1;
    float a2, f2;
    float noiseStdDev;
    float anomalyProb;
    float anomalyMin;
    float anomalyMax;
};

struct SignalDef
{
    const char* name;
    float components[4][2];
    int numComponents;
};

float generateSignal(float t);
float generateNoisySignal(float t, const struct SignalConfig& config, int* anomalyOut);
float generateSignalFromDef(float t, const struct SignalDef& def);
