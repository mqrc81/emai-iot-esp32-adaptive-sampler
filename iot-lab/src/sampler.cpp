#include "sampler.hpp"
#include "signal.hpp"
#include "fft.hpp"
#include <Arduino.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "esp_timer.h"

#define FFT_SIZE 256

// Returns elapsed milliseconds since start
static float elapsedMs(uint64_t startUs) {
    return (esp_timer_get_time() - startUs) / 1000.0f;
}

WindowResult computeWindow(float sampleRateHz, float windowSecs) {
    int totalSamples = (int) (sampleRateHz * windowSecs);
    float *buffer = (float *) malloc(totalSamples * sizeof(float));

    uint64_t start = esp_timer_get_time();

    // Collect samples
    for (int i = 0; i < totalSamples; i++) {
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
