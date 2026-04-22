#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "signal.h"
#include "filter.h"

#define SAMPLE_RATE_HZ  1000.0f
#define NUM_SAMPLES     1000
#define ZSCORE_THRESH   3.0f
#define HAMPEL_THRESH   3.0f

static float elapsedMs(struct timespec start)
{
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start.tv_sec) * 1000.0f +
        (now.tv_nsec - start.tv_nsec) / 1e6f;
}

struct TradeoffResult
{
    int windowSize;
    float zTPR, zFPR, zMeanError, zTimeMs;
    float hTPR, hFPR, hMeanError, hTimeMs;
    size_t memoryBytes; // memory used by filter window
};

TradeoffResult evaluateWindowSize(int windowSize, float anomalyProb)
{
    srand(42);

    struct SignalConfig config;
    config.a1 = 2.0f;
    config.f1 = 3.0f;
    config.a2 = 4.0f;
    config.f2 = 5.0f;
    config.noiseStdDev = 0.2f;
    config.anomalyProb = anomalyProb;
    config.anomalyMin = 5.0f;
    config.anomalyMax = 15.0f;

    float* samples = (float*)malloc(NUM_SAMPLES * sizeof(float));
    float* clean = (float*)malloc(NUM_SAMPLES * sizeof(float));
    int* gt = (int*)malloc(NUM_SAMPLES * sizeof(int));

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        float t = i / SAMPLE_RATE_HZ;
        clean[i] = generateSignal(t);
        samples[i] = generateNoisySignal(t, config, &gt[i]);
    }

    int half = windowSize / 2;
    int zTP = 0, zFP = 0, zFN = 0, zTN = 0;
    int hTP = 0, hFP = 0, hFN = 0, hTN = 0;
    float zError = 0, hError = 0;

    // Time Z-score
    struct timespec zStart;
    clock_gettime(CLOCK_MONOTONIC, &zStart);
    for (int i = half; i < NUM_SAMPLES - half; i++)
    {
        const float* window = &samples[i - half];
        FilterResult r = zscoreFilter(window, windowSize, half, ZSCORE_THRESH);
        int g = gt[i];
        if (g == 1 && r.flagged == 1) zTP++;
        if (g == 0 && r.flagged == 1) zFP++;
        if (g == 1 && r.flagged == 0) zFN++;
        if (g == 0 && r.flagged == 0) zTN++;
        zError += fabsf(r.cleaned - clean[i]);
    }
    float zTimeMs = elapsedMs(zStart);

    // Time Hampel
    struct timespec hStart;
    clock_gettime(CLOCK_MONOTONIC, &hStart);
    for (int i = half; i < NUM_SAMPLES - half; i++)
    {
        const float* window = &samples[i - half];
        FilterResult r = hampelFilter(window, windowSize, half, HAMPEL_THRESH);
        int g = gt[i];
        if (g == 1 && r.flagged == 1) hTP++;
        if (g == 0 && r.flagged == 1) hFP++;
        if (g == 1 && r.flagged == 0) hFN++;
        if (g == 0 && r.flagged == 0) hTN++;
        hError += fabsf(r.cleaned - clean[i]);
    }
    float hTimeMs = elapsedMs(hStart);

    int evaluated = NUM_SAMPLES - 2 * half;

    struct TradeoffResult result;
    result.windowSize = windowSize;
    result.zTPR = (float)zTP / (zTP + zFN + 1e-6f);
    result.zFPR = (float)zFP / (zFP + zTN + 1e-6f);
    result.zMeanError = zError / evaluated;
    result.zTimeMs = zTimeMs;
    result.hTPR = (float)hTP / (hTP + hFN + 1e-6f);
    result.hFPR = (float)hFP / (hFP + hTN + 1e-6f);
    result.hMeanError = hError / evaluated;
    result.hTimeMs = hTimeMs;
    result.memoryBytes = windowSize * sizeof(float); // window buffer size

    free(samples);
    free(clean);
    free(gt);
    return result;
}

int main()
{
    // Odd window sizes spanning small to large
    int windows[] = {5, 11, 21, 51, 101, 201};
    int nWindows = 6;
    float anomalyProb = 0.05f; // p=5% is the interesting middle case

    printf("Testing at anomaly rate p=%.0f%%\n\n", anomalyProb * 100);

    printf("=== Z-score Filter ===\n");
    printf("%-8s %-8s %-8s %-12s %-12s %-12s\n",
           "W", "TPR", "FPR", "MeanErr", "Time(ms)", "Mem(bytes)");
    for (int i = 0; i < nWindows; i++)
    {
        TradeoffResult r = evaluateWindowSize(windows[i], anomalyProb);
        printf("%-8d %-8.3f %-8.3f %-12.6f %-12.4f %-12zu\n",
               r.windowSize, r.zTPR, r.zFPR,
               r.zMeanError, r.zTimeMs, r.memoryBytes);
    }

    printf("\n=== Hampel Filter ===\n");
    printf("%-8s %-8s %-8s %-12s %-12s %-12s\n",
           "W", "TPR", "FPR", "MeanErr", "Time(ms)", "Mem(bytes)");
    for (int i = 0; i < nWindows; i++)
    {
        TradeoffResult r = evaluateWindowSize(windows[i], anomalyProb);
        printf("%-8d %-8.3f %-8.3f %-12.6f %-12.4f %-12zu\n",
               r.windowSize, r.hTPR, r.hFPR,
               r.hMeanError, r.hTimeMs, r.memoryBytes);
    }

    return 0;
}
