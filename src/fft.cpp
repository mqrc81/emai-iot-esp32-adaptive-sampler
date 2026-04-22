#include "fft.h"
#include "kiss_fftr.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

FFTResult computeFFT(const float* samples, int size, float sampleRateHz)
{
    kiss_fftr_cfg cfg = kiss_fftr_alloc(size, 0, nullptr, nullptr);

    // Input: real-valued samples
    kiss_fft_scalar* in = (kiss_fft_scalar*)malloc(size * sizeof(kiss_fft_scalar));
    // Output: complex frequency bins (size/2 + 1)
    kiss_fft_cpx* out = (kiss_fft_cpx*)malloc((size / 2 + 1) * sizeof(kiss_fft_cpx));

    for (int i = 0; i < size; i++)
        in[i] = samples[i];

    kiss_fftr(cfg, in, out);

    // Find dominant frequency (skip bin 0 = DC component)
    float maxMag = 0.0f;
    int dominantBin = 1;
    for (int i = 1; i <= size / 2; i++)
    {
        float mag = sqrtf(out[i].r * out[i].r + out[i].i * out[i].i);
        if (mag > maxMag)
        {
            maxMag = mag;
            dominantBin = i;
        }
    }

    float freqResolution = sampleRateHz / size;
    float dominantFreq = dominantBin * freqResolution;

    free(in);
    free(out);
    kiss_fftr_free(cfg);

    FFTResult result;
    result.dominantFrequency = dominantFreq;
    result.optimalSampleRate = 2.0f * dominantFreq; // Nyquist
    return result;
}
