#include <stdio.h>
#include <stdlib.h>
#include "signal.h"

#define SAMPLE_RATE_HZ 1000.0f
#define NUM_SAMPLES    1000  // 1 second

int main()
{
    srand(42); // fixed seed for reproducibility

    SignalConfig config;
    config.a1 = 2.0f;
    config.f1 = 3.0f;
    config.a2 = 4.0f;
    config.f2 = 5.0f;
    config.noiseStdDev = 0.2f;
    config.anomalyProb = 0.02f;
    config.anomalyMin = 5.0f;
    config.anomalyMax = 15.0f;

    int anomalyCount = 0;

    // CSV: t, clean, noisy, is_anomaly
    printf("t,clean,noisy,is_anomaly\n");
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        float t = i / SAMPLE_RATE_HZ;
        float clean = generateSignal(t);
        int isAnomaly = 0;
        float noisy = generateNoisySignal(t, config, &isAnomaly);
        anomalyCount += isAnomaly;
        printf("%.4f,%.4f,%.4f,%d\n", t, clean, noisy, isAnomaly);
    }

    fprintf(stderr, "Total anomalies injected: %d / %d (%.1f%%)\n",
            anomalyCount, NUM_SAMPLES,
            100.0f * anomalyCount / NUM_SAMPLES);

    return 0;
}
