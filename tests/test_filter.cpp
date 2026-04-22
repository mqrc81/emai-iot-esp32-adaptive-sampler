#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "signal.h"
#include "filter.h"

#define SAMPLE_RATE_HZ  1000.0f
#define NUM_SAMPLES     1000
#define WINDOW_SIZE     21
#define ZSCORE_THRESH   3.0f
#define HAMPEL_THRESH   3.0f

struct FilterStats
{
    int TP, FP, FN, TN;
    float TPR, FPR;
    float meanError; // mean |cleaned - clean| across window
};

FilterStats evaluateFilters(float anomalyProb, int useZscore)
{
    srand(42); // fixed seed for reproducibility

    SignalConfig config;
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

    int half = WINDOW_SIZE / 2;
    FilterStats stats = {0};
    float totalError = 0.0f;
    int errorCount = 0;

    for (int i = half; i < NUM_SAMPLES - half; i++)
    {
        const float* window = &samples[i - half];

        FilterResult r = useZscore
                             ? zscoreFilter(window, WINDOW_SIZE, half, ZSCORE_THRESH)
                             : hampelFilter(window, WINDOW_SIZE, half, HAMPEL_THRESH);

        int g = gt[i];
        if (g == 1 && r.flagged == 1) stats.TP++;
        if (g == 0 && r.flagged == 1) stats.FP++;
        if (g == 1 && r.flagged == 0) stats.FN++;
        if (g == 0 && r.flagged == 0) stats.TN++;

        // Mean error: how close is cleaned value to true clean signal
        totalError += fabsf(r.cleaned - clean[i]);
        errorCount++;
    }

    stats.TPR = (float)stats.TP / (stats.TP + stats.FN + 1e-6f);
    stats.FPR = (float)stats.FP / (stats.FP + stats.TN + 1e-6f);
    stats.meanError = totalError / errorCount;

    free(samples);
    free(clean);
    free(gt);
    return stats;
}

int main()
{
    float probs[] = {0.01f, 0.05f, 0.10f};
    int nProbs = 3;

    printf("=== Z-score Filter (k=%.1f, W=%d) ===\n", ZSCORE_THRESH, WINDOW_SIZE);
    printf("%-8s %-6s %-6s %-6s %-6s %-6s %-6s %-12s\n",
           "p", "TP", "FP", "FN", "TN", "TPR", "FPR", "MeanErr");
    for (int i = 0; i < nProbs; i++)
    {
        FilterStats s = evaluateFilters(probs[i], 1);
        printf("%-8.2f %-6d %-6d %-6d %-6d %-6.3f %-6.3f %-12.6f\n",
               probs[i], s.TP, s.FP, s.FN, s.TN, s.TPR, s.FPR, s.meanError);
    }

    printf("\n=== Hampel Filter (k=%.1f, W=%d) ===\n", HAMPEL_THRESH, WINDOW_SIZE);
    printf("%-8s %-6s %-6s %-6s %-6s %-6s %-6s %-12s\n",
           "p", "TP", "FP", "FN", "TN", "TPR", "FPR", "MeanErr");
    for (int i = 0; i < nProbs; i++)
    {
        FilterStats s = evaluateFilters(probs[i], 0);
        printf("%-8.2f %-6d %-6d %-6d %-6d %-6.3f %-6.3f %-12.6f\n",
               probs[i], s.TP, s.FP, s.FN, s.TN, s.TPR, s.FPR, s.meanError);
    }

    return 0;
}
