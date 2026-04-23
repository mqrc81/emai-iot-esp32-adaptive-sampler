#include <stdio.h>
#include "signal.h"
#include "fft.h"
#include "sampler.h"
#include <math.h>

#define MAX_SAMPLE_RATE  1000.0f
#define WINDOW_SECS      5.0f
#define FFT_SIZE         2048

int main()
{
    // Step 1: FFT at max rate to find optimal rate
    float fftBuffer[FFT_SIZE];
    for (int i = 0; i < FFT_SIZE; i++)
        fftBuffer[i] = generateSignal(i / MAX_SAMPLE_RATE);

    FFTResult fft = computeFFT(fftBuffer, FFT_SIZE, MAX_SAMPLE_RATE);
    printf("=== FFT ===\n");
    printf("Dominant freq    : %.2f Hz\n", fft.dominantFrequency);
    printf("Optimal rate     : %.2f Hz\n", fft.optimalSampleRate);

    // Step 2: Window at max (oversampled) rate
    printf("\n=== Window @ max rate (%.0f Hz) ===\n", MAX_SAMPLE_RATE);
    WindowResult oversampled = computeWindow(MAX_SAMPLE_RATE, WINDOW_SECS);
    printf("Average          : %.6f\n", oversampled.average);
    printf("Samples          : %d\n", oversampled.sampleCount);
    printf("Duration         : %.3f ms\n", oversampled.windowDurationMs);

    // Step 3: Window at adaptive (optimal) rate
    printf("\n=== Window @ adaptive rate (%.2f Hz) ===\n", fft.optimalSampleRate);
    WindowResult adaptive = computeWindow(fft.optimalSampleRate, WINDOW_SECS);
    printf("Average          : %.6f\n", adaptive.average);
    printf("Samples          : %d\n", adaptive.sampleCount);
    printf("Duration         : %.3f ms\n", adaptive.windowDurationMs);

    // Step 4: Key metrics
    printf("\n=== Metrics ===\n");
    printf("Sample reduction : %.1fx\n", (float)oversampled.sampleCount / adaptive.sampleCount);
    printf("Avg difference   : %.6f\n", fabsf(oversampled.average - adaptive.average));

    return 0;
}
