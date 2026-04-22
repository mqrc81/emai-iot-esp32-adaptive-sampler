#include <stdio.h>
#include "signal.h"
#include "fft.h"

#define SAMPLE_RATE_HZ 100.0f
#define FFT_SIZE 256  // must be power of 2

int main()
{
    float samples[FFT_SIZE];

    // Collect one window of samples
    for (int i = 0; i < FFT_SIZE; i++)
    {
        float t = i / SAMPLE_RATE_HZ;
        samples[i] = generateSignal(t);
    }

    FFTResult result = computeFFT(samples, FFT_SIZE, SAMPLE_RATE_HZ);

    printf("Dominant frequency : %.2f Hz\n", result.dominantFrequency);
    printf("Optimal sample rate: %.2f Hz\n", result.optimalSampleRate);
    printf("Original sample rate: %.2f Hz\n", SAMPLE_RATE_HZ);
    printf("Rate reduction factor: %.1fx\n", SAMPLE_RATE_HZ / result.optimalSampleRate);

    return 0;
}
