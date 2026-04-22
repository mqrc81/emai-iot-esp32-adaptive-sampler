#include <stdio.h>
#include <stdlib.h>
#include "signal.h"
#include "fft.h"
#include "filter.h"

#define SAMPLE_RATE_HZ  1000.0f
#define FFT_SIZE        2048
#define WINDOW_SIZE     21
#define ZSCORE_THRESH   3.0f
#define HAMPEL_THRESH   3.0f

// Collect FFT_SIZE samples, optionally filter them, then run FFT
FFTResult runFFT(float anomalyProb, int filterType)
{
    // filterType: 0 = none, 1 = zscore, 2 = hampel
    srand(42);

    SignalConfig config;
    config.a1 = 2.0f;
    config.f1 = 3.0f;
    config.a2 = 4.0f;
    config.f2 = 5.0f;
    config.noiseStdDev = 0.2f;
    config.anomalyProb = anomalyProb;
    config.anomalyMin = 5.0f;
    config.anomalyMax = 15.0f;

    float* samples = (float*)malloc(FFT_SIZE * sizeof(float));
    float* filtered = (float*)malloc(FFT_SIZE * sizeof(float));

    for (int i = 0; i < FFT_SIZE; i++)
    {
        float t = i / SAMPLE_RATE_HZ;
        samples[i] = generateNoisySignal(t, config, nullptr);
    }

    // Apply filter if requested
    int half = WINDOW_SIZE / 2;
    for (int i = 0; i < FFT_SIZE; i++)
    {
        if (filterType == 0 || i < half || i >= FFT_SIZE - half)
        {
            // No filter or boundary — use raw sample
            filtered[i] = samples[i];
        }
        else
        {
            const float* window = &samples[i - half];
            FilterResult r = (filterType == 1)
                                 ? zscoreFilter(window, WINDOW_SIZE, half, ZSCORE_THRESH)
                                 : hampelFilter(window, WINDOW_SIZE, half, HAMPEL_THRESH);
            filtered[i] = r.cleaned;
        }
    }

    FFTResult result = computeFFT(filtered, FFT_SIZE, SAMPLE_RATE_HZ);

    free(samples);
    free(filtered);
    return result;
}

int main()
{
    float probs[] = {0.01f, 0.05f, 0.10f};
    int nProbs = 3;

    printf("%-6s %-12s %-12s %-12s %-12s %-12s %-12s %-12s\n",
           "p", "Unfiltered", "ZScore", "Hampel",
           "Rate_Unfilt", "Rate_ZScore", "Rate_Hampel", "TrueRate");

    for (int i = 0; i < nProbs; i++)
    {
        FFTResult unfiltered = runFFT(probs[i], 0);
        FFTResult zscore = runFFT(probs[i], 1);
        FFTResult hampel = runFFT(probs[i], 2);

        printf("%-6.2f %-12.3f %-12.3f %-12.3f %-12.3f %-12.3f %-12.3f %-12.3f\n",
               probs[i],
               unfiltered.dominantFrequency,
               zscore.dominantFrequency,
               hampel.dominantFrequency,
               unfiltered.optimalSampleRate,
               zscore.optimalSampleRate,
               hampel.optimalSampleRate,
               9.77f); // baseline from clean signal FFT
    }

    printf("\n=== Top 5 bins for p=10%% unfiltered ===\n");
    srand(42);
    SignalConfig config;
    config.a1 = 2.0f;
    config.f1 = 3.0f;
    config.a2 = 4.0f;
    config.f2 = 5.0f;
    config.noiseStdDev = 0.2f;
    config.anomalyProb = 0.10f;
    config.anomalyMin = 5.0f;
    config.anomalyMax = 15.0f;

    float* debug = (float*)malloc(FFT_SIZE * sizeof(float));
    for (int i = 0; i < FFT_SIZE; i++)
        debug[i] = generateNoisySignal(i / SAMPLE_RATE_HZ, config, nullptr);

    printTopBins(debug, FFT_SIZE, SAMPLE_RATE_HZ, 5);
    free(debug);

    return 0;
}
