#include <stdio.h>
#include <stdlib.h>
#include "signal.h"
#include "filter.h"

#define SAMPLE_RATE_HZ  1000.0f
#define NUM_SAMPLES     1000
#define WINDOW_SIZE     21      // odd number, center sample at index 10
#define ZSCORE_THRESH   3.0f    // standard choice from literature
#define HAMPEL_THRESH   3.0f    // standard choice from literature

int main()
{
    srand(42);

    SignalConfig config;
    config.a1 = 2.0f;
    config.f1 = 3.0f;
    config.a2 = 4.0f;
    config.f2 = 5.0f;
    config.noiseStdDev = 0.2f;
    config.anomalyProb = 0.02f;
    config.anomalyMin = 5.0f;
    config.anomalyMax = 15.0f;

    // Generate full signal + ground truth
    float* samples = (float*)malloc(NUM_SAMPLES * sizeof(float));
    int* gtAnomaly = (int*)malloc(NUM_SAMPLES * sizeof(int));

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        float t = i / SAMPLE_RATE_HZ;
        samples[i] = generateNoisySignal(t, config, &gtAnomaly[i]);
    }

    // Per-filter counters
    int half = WINDOW_SIZE / 2;
    int zTP = 0, zFP = 0, zFN = 0, zTN = 0;
    int hTP = 0, hFP = 0, hFN = 0, hTN = 0;

    printf("i,sample,gt,z_flagged,h_flagged\n");

    for (int i = half; i < NUM_SAMPLES - half; i++)
    {
        const float* window = &samples[i - half];

        FilterResult zr = zscoreFilter(window, WINDOW_SIZE, half, ZSCORE_THRESH);
        FilterResult hr = hampelFilter(window, WINDOW_SIZE, half, HAMPEL_THRESH);

        int gt = gtAnomaly[i];

        // Z-score confusion matrix
        if (gt == 1 && zr.flagged == 1) zTP++;
        if (gt == 0 && zr.flagged == 1) zFP++;
        if (gt == 1 && zr.flagged == 0) zFN++;
        if (gt == 0 && zr.flagged == 0) zTN++;

        // Hampel confusion matrix
        if (gt == 1 && hr.flagged == 1) hTP++;
        if (gt == 0 && hr.flagged == 1) hFP++;
        if (gt == 1 && hr.flagged == 0) hFN++;
        if (gt == 0 && hr.flagged == 0) hTN++;

        printf("%d,%.4f,%d,%d,%d\n", i, samples[i], gt, zr.flagged, hr.flagged);
    }

    // TPR = TP / (TP + FN),  FPR = FP / (FP + TN)
    printf("\n=== Z-score Filter (k=%.1f, W=%d) ===\n", ZSCORE_THRESH, WINDOW_SIZE);
    printf("TP=%d FP=%d FN=%d TN=%d\n", zTP, zFP, zFN, zTN);
    printf("TPR: %.3f\n", (float)zTP / (zTP + zFN + 1e-6f));
    printf("FPR: %.3f\n", (float)zFP / (zFP + zTN + 1e-6f));

    printf("\n=== Hampel Filter (k=%.1f, W=%d) ===\n", HAMPEL_THRESH, WINDOW_SIZE);
    printf("TP=%d FP=%d FN=%d TN=%d\n", hTP, hFP, hFN, hTN);
    printf("TPR: %.3f\n", (float)hTP / (hTP + hFN + 1e-6f));
    printf("FPR: %.3f\n", (float)hFP / (hFP + hTN + 1e-6f));

    free(samples);
    free(gtAnomaly);
    return 0;
}
