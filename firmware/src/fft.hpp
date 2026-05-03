#pragma once

struct FFTResult {
    float dominantFrequency; // Hz
    float optimalSampleRate; // Hz (2 * dominantFrequency, Nyquist)
};

// samples: raw signal buffer, size must be power of 2
// sampleRateHz: the rate at which samples were collected
struct FFTResult computeFFT(const float *samples, int size, float sampleRateHz);
