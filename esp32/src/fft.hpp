#pragma once
#include <Arduino.h>

struct FFTResult {
    float dominantFrequency;
    float optimalSampleRate;
};

// samples: raw signal buffer, size must be power of 2
// sampleRateHz: rate at which samples were collected
FFTResult computeFFT(const float *samples, int size, float sampleRateHz);
