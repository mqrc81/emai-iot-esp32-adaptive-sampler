#include "sampler.h"
#include "signal.h"
#include "fft.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define FFT_SIZE 256

// Returns elapsed milliseconds since start
static float elapsedMs(struct timespec start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start.tv_sec) * 1000.0f +
        (now.tv_nsec - start.tv_nsec) / 1e6f;
}

WindowResult computeWindow(float sampleRateHz, float windowSecs)
{
    int totalSamples = (int)(sampleRateHz * windowSecs);
    float* buffer = (float*)malloc(totalSamples * sizeof(float));

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Collect samples
    for (int i = 0; i < totalSamples; i++)
    {
        float t = i / sampleRateHz;
        buffer[i] = generateSignal(t);
    }

    // Compute average
    float sum = 0.0f;
    for (int i = 0; i < totalSamples; i++)
        sum += buffer[i];
    float average = sum / totalSamples;

    float duration = elapsedMs(start);

    free(buffer);

    WindowResult result;
    result.average = average;
    result.windowDurationMs = duration;
    result.sampleCount = totalSamples;
    return result;
}
