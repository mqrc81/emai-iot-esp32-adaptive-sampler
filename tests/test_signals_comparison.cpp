#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "signal.h"
#include "fft.h"
#include "sampler.h"

#define SAMPLE_RATE_HZ  1000.0f
#define FFT_SIZE        2048
#define WINDOW_SECS     5.0f

static float elapsedMs(struct timespec start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start.tv_sec) * 1000.0f +
        (now.tv_nsec - start.tv_nsec) / 1e6f;
}

int main()
{
    // Define the three signals
    SignalDef signals[3];

    // Signal 1: original
    signals[0].name = "2sin(2pi*3t) + 4sin(2pi*5t)";
    signals[0].numComponents = 2;
    signals[0].components[0][0] = 2.0f;
    signals[0].components[0][1] = 3.0f;
    signals[0].components[1][0] = 4.0f;
    signals[0].components[1][1] = 5.0f;

    // Signal 2: single low frequency
    signals[1].name = "4sin(2pi*2t)";
    signals[1].numComponents = 1;
    signals[1].components[0][0] = 4.0f;
    signals[1].components[0][1] = 2.0f;

    // Signal 3: high frequency dominant
    signals[2].name = "2sin(2pi*10t) + 3sin(2pi*45t)";
    signals[2].numComponents = 2;
    signals[2].components[0][0] = 2.0f;
    signals[2].components[0][1] = 10.0f;
    signals[2].components[1][0] = 3.0f;
    signals[2].components[1][1] = 45.0f;

    printf("%-40s %-12s %-12s %-12s %-12s %-12s\n",
           "Signal", "DomFreq(Hz)", "AdaptRate", "Reduction",
           "EnergySav%", "Window(ms)");

    for (int s = 0; s < 3; s++)
    {
        // Collect FFT buffer
        float* fftBuf = (float*)malloc(FFT_SIZE * sizeof(float));
        for (int i = 0; i < FFT_SIZE; i++)
            fftBuf[i] = generateSignalFromDef(i / SAMPLE_RATE_HZ, signals[s]);

        FFTResult fft = computeFFT(fftBuf, FFT_SIZE, SAMPLE_RATE_HZ);
        free(fftBuf);

        // Compute window at adaptive rate
        // We need a version of computeWindow that uses SignalDef
        int totalSamples = (int)(fft.optimalSampleRate * WINDOW_SECS);
        float* winBuf = (float*)malloc(totalSamples * sizeof(float));

        struct timespec wStart;
        clock_gettime(CLOCK_MONOTONIC, &wStart);

        for (int i = 0; i < totalSamples; i++)
        {
            float t = i / fft.optimalSampleRate;
            winBuf[i] = generateSignalFromDef(t, signals[s]);
        }
        float sum = 0.0f;
        for (int i = 0; i < totalSamples; i++) sum += winBuf[i];
        float windowMs = elapsedMs(wStart);
        free(winBuf);

        float reduction = SAMPLE_RATE_HZ / fft.optimalSampleRate;
        float energySaving = (1.0f - 1.0f / reduction) * 100.0f;

        printf("%-40s %-12.3f %-12.3f %-11.1fx %-12.1f %-12.4f\n",
               signals[s].name,
               fft.dominantFrequency,
               fft.optimalSampleRate,
               reduction,
               energySaving,
               windowMs);
    }

    return 0;
}
