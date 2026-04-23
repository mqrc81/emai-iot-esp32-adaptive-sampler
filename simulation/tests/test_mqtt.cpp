#include <stdio.h>
#include <time.h>
#include "signal.h"
#include "fft.h"
#include "sampler.h"
#include "mqtt_client.h"

#define MAX_SAMPLE_RATE  1000.0f
#define WINDOW_SECS      5.0f
#define FFT_SIZE         2048
#define NUM_WINDOWS      10      // run multiple windows for stable averages

static float elapsedMs(struct timespec start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start.tv_sec) * 1000.0f +
        (now.tv_nsec - start.tv_nsec) / 1e6f;
}

int main()
{
    // FFT to find optimal rate
    float fftBuffer[FFT_SIZE];
    for (int i = 0; i < FFT_SIZE; i++)
        fftBuffer[i] = generateSignal(i / MAX_SAMPLE_RATE);
    FFTResult fft = computeFFT(fftBuffer, FFT_SIZE, MAX_SAMPLE_RATE);

    // Connect once before tests
    MQTTConfig config;
    config.host = "localhost";
    config.port = 1883;
    config.topic = "iot/window";
    config.clientId = "iot-sampler-001";

    MQTTClient client = mqttConnect(config);

    printf("\n=== Configuration ===\n");
    printf("Max sample rate     : %.2f Hz\n", MAX_SAMPLE_RATE);
    printf("Adaptive rate       : %.2f Hz\n", fft.optimalSampleRate);
    printf("Window duration     : %.1f s\n", WINDOW_SECS);
    printf("Number of windows   : %d\n\n", NUM_WINDOWS);

    printf("=== Per-Window Metrics ===\n");
    printf("%-8s %-12s %-12s %-12s %-12s\n",
           "Window", "Avg", "Samples", "Compute(ms)", "E2E(ms)");

    float totalE2E = 0.0f;
    float totalCompute = 0.0f;

    for (int w = 0; w < NUM_WINDOWS; w++)
    {
        // Start e2e timer at signal generation
        struct timespec e2eStart;
        clock_gettime(CLOCK_MONOTONIC, &e2eStart);

        WindowResult window = computeWindow(fft.optimalSampleRate, WINDOW_SECS);
        mqttPublishWindow(client, window, fft.optimalSampleRate);

        float e2eMs = elapsedMs(e2eStart);
        totalE2E += e2eMs;
        totalCompute += window.windowDurationMs;

        printf("%-8d %-12.6f %-12d %-12.3f %-12.3f\n",
               w + 1,
               window.average,
               window.sampleCount,
               window.windowDurationMs,
               e2eMs
        );
    }

    // Disconnect after all windows
    mqttDisconnect(client);

    printf("\n=== Summary ===\n");
    printf("Avg compute time    : %.3f ms\n", totalCompute / NUM_WINDOWS);
    printf("Avg E2E latency     : %.3f ms\n", totalE2E / NUM_WINDOWS);

    // Energy & data volume
    float rateRatio = MAX_SAMPLE_RATE / fft.optimalSampleRate;
    printf("\n=== Efficiency Metrics ===\n");
    printf("Sample reduction    : %.1fx\n", rateRatio);
    printf("Energy saving       : %.1f%%\n", (1.0f - 1.0f / rateRatio) * 100.0f);
    printf("Data volume saving  : %.1f%%\n", (1.0f - 1.0f / rateRatio) * 100.0f);

    // Bytes per window
    int bytesOversampled = (int)(MAX_SAMPLE_RATE * WINDOW_SECS) * sizeof(float);
    int bytesAdaptive = (int)(fft.optimalSampleRate * WINDOW_SECS) * sizeof(float);
    printf("Bytes/window (max)  : %d\n", bytesOversampled);
    printf("Bytes/window (adap) : %d\n", bytesAdaptive);

    return 0;
}
