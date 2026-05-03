#include <Arduino.h>
#include <math.h>
#include "signal.hpp"
#include "fft.hpp"
#include "sampler.hpp"
#include "filter.hpp"
#include "esp_timer.h"

// ======= CONFIG =======

#define MAX_SAMPLE_RATE   1000.0f
#define FFT_SIZE          2048
#define WINDOW_SECS       5.0f
#define NUM_WINDOWS       5
#define FILTER_WINDOW     21
#define ZSCORE_THRESH     3.0f
#define HAMPEL_THRESH     3.0f
#define BENCHMARK_SAMPLES 10000  // for max rate measurement

// Anomaly config (p=2% baseline)
static SignalConfig noisyConfig = {
    .a1 = 2.0f, .f1 = 3.0f,
    .a2 = 4.0f, .f2 = 5.0f,
    .noiseStdDev = 0.2f,
    .anomalyProb = 0.02f,
    .anomalyMin = 5.0f,
    .anomalyMax = 15.0f
};

// ======= HELPERS =======

static float elapsedMs(uint64_t startUs) {
    return (esp_timer_get_time() - startUs) / 1000.0f;
}

// Print a single JSON line; bridge forwards each line to MQTT
static void printJSON(const char *buf) {
    Serial0.println(buf);
}

// ======= PHASES =======

// --- 1. Max sampling rate benchmark ---
void benchmarkMaxRate() {
    Serial0.println("{\"phase\":\"BENCHMARK_START\"}");

    uint64_t start = esp_timer_get_time();
    volatile float sink = 0.0f; // prevent compiler optimising away the loop

    for (int i = 0; i < BENCHMARK_SAMPLES; i++) {
        float t = i / MAX_SAMPLE_RATE;
        sink = generateSignal(t); // represents an ADC read + signal generation
    }

    float elapsedSec = (esp_timer_get_time() - start) / 1e6f;
    float achievedRate = BENCHMARK_SAMPLES / elapsedSec;

    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"phase\":\"BENCHMARK\",\"samples\":%d,\"elapsed_s\":%.4f,\"achieved_hz\":%.2f}",
             BENCHMARK_SAMPLES, elapsedSec, achievedRate);
    printJSON(buf);
}

// --- 2. FFT on clean signal ---
FFTResult runFFT(const struct SignalDef *sigDef) {
    float *buf = (float *) malloc(FFT_SIZE * sizeof(float));
    for (int i = 0; i < FFT_SIZE; i++) {
        float t = i / MAX_SAMPLE_RATE;
        buf[i] = sigDef
                     ? generateSignalFromDef(t, *sigDef)
                     : generateSignal(t);
    }

    FFTResult result = computeFFT(buf, FFT_SIZE, MAX_SAMPLE_RATE);
    free(buf);
    return result;
}

// --- 3. Windowed average (clean signal) ---
void runWindowedAverage(float adaptiveRate, const char *label, const struct SignalDef *sigDef) {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"phase\":\"%s_START\",\"adaptive_rate\":%.2f,\"window_secs\":%.1f,\"num_windows\":%d}",
             label, adaptiveRate, WINDOW_SECS, NUM_WINDOWS);
    printJSON(buf);

    int totalSamples = (int) (adaptiveRate * WINDOW_SECS);
    float *samples = (float *) malloc(totalSamples * sizeof(float));

    for (int w = 0; w < NUM_WINDOWS; w++) {
        uint64_t e2eStart = esp_timer_get_time();

        // Collect samples
        uint64_t computeStart = esp_timer_get_time();
        for (int i = 0; i < totalSamples; i++) {
            float t = i / adaptiveRate;
            samples[i] = sigDef
                             ? generateSignalFromDef(t, *sigDef)
                             : generateSignal(t);
        }

        // Compute average
        float sum = 0.0f;
        for (int i = 0; i < totalSamples; i++) sum += samples[i];
        float average = sum / totalSamples;
        float computeMs = elapsedMs(computeStart);
        float e2eMs = elapsedMs(e2eStart);

        snprintf(buf, sizeof(buf),
                 "{\"phase\":\"%s\",\"window\":%d,\"average\":%.6f,\"samples\":%d,\"compute_ms\":%.4f,\"e2e_ms\":%.4f}",
                 label, w + 1, average, totalSamples, computeMs, e2eMs);
        printJSON(buf);
    }

    free(samples);
}

// --- 4. Windowed average with filter (noisy signal) ---
void runWindowedFiltered(float adaptiveRate, const char *label, int useZscore) {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"phase\":\"%s_START\",\"adaptive_rate\":%.2f,\"window_size\":%d}",
             label, adaptiveRate, FILTER_WINDOW);
    printJSON(buf);

    int totalSamples = (int) (adaptiveRate * WINDOW_SECS);
    float *samples = (float *) malloc(totalSamples * sizeof(float));
    int *gt = (int *) malloc(totalSamples * sizeof(int));

    int half = FILTER_WINDOW / 2;
    int tp = 0, fp = 0, fn = 0, tn = 0;
    float totalError = 0.0f;

    for (int w = 0; w < NUM_WINDOWS; w++) {
        uint64_t e2eStart = esp_timer_get_time();

        // Generate noisy samples
        for (int i = 0; i < totalSamples; i++) {
            float t = i / adaptiveRate;
            samples[i] = generateNoisySignal(t, noisyConfig, &gt[i]);
        }

        // Apply filter and compute average
        uint64_t computeStart = esp_timer_get_time();
        float sum = 0.0f;
        int counted = 0;

        for (int i = 0; i < totalSamples; i++) {
            float cleaned;
            int flagged = 0;

            if (i >= half && i < totalSamples - half) {
                const float *window = &samples[i - half];
                FilterResult r = useZscore
                                     ? zscoreFilter(window, FILTER_WINDOW, half, ZSCORE_THRESH)
                                     : hampelFilter(window, FILTER_WINDOW, half, HAMPEL_THRESH);
                cleaned = r.cleaned;
                flagged = r.flagged;

                // Confusion matrix
                if (gt[i] == 1 && flagged == 1) tp++;
                if (gt[i] == 0 && flagged == 1) fp++;
                if (gt[i] == 1 && flagged == 0) fn++;
                if (gt[i] == 0 && flagged == 0) tn++;

                // Error vs clean signal
                float trueVal = generateSignal(i / adaptiveRate);
                totalError += fabsf(cleaned - trueVal);
                counted++;
            } else {
                cleaned = samples[i];
            }
            sum += cleaned;
        }

        float average = sum / totalSamples;
        float computeMs = elapsedMs(computeStart);
        float e2eMs = elapsedMs(e2eStart);

        snprintf(buf, sizeof(buf),
                 "{\"phase\":\"%s\",\"window\":%d,\"average\":%.6f,\"compute_ms\":%.4f,\"e2e_ms\":%.4f}",
                 label, w + 1, average, computeMs, e2eMs);
        printJSON(buf);
    }

    // Summary
    float tpr = (float) tp / (tp + fn + 1e-6f);
    float fpr = (float) fp / (fp + tn + 1e-6f);
    float meanError = totalError / ((NUM_WINDOWS * (totalSamples - 2 * half)) + 1e-6f);

    snprintf(buf, sizeof(buf),
             "{\"phase\":\"%s_SUMMARY\",\"tp\":%d,\"fp\":%d,\"fn\":%d,\"tn\":%d,\"tpr\":%.4f,\"fpr\":%.4f,\"mean_error\":%.6f}",
             label, tp, fp, fn, tn, tpr, fpr, meanError);
    printJSON(buf);

    free(samples);
    free(gt);
}

// --- 5. 3 signals comparison ---
void runSignalsComparison() {
    Serial0.println("{\"phase\":\"SIGNALS_COMPARISON_START\"}");

    SignalDef signals[3];

    signals[0].name = "2sin(2pi*3t)+4sin(2pi*5t)";
    signals[0].numComponents = 2;
    signals[0].components[0][0] = 2.0f;
    signals[0].components[0][1] = 3.0f;
    signals[0].components[1][0] = 4.0f;
    signals[0].components[1][1] = 5.0f;

    signals[1].name = "4sin(2pi*2t)";
    signals[1].numComponents = 1;
    signals[1].components[0][0] = 4.0f;
    signals[1].components[0][1] = 2.0f;

    signals[2].name = "2sin(2pi*10t)+3sin(2pi*45t)";
    signals[2].numComponents = 2;
    signals[2].components[0][0] = 2.0f;
    signals[2].components[0][1] = 10.0f;
    signals[2].components[1][0] = 3.0f;
    signals[2].components[1][1] = 45.0f;

    char buf[256];
    for (int s = 0; s < 3; s++) {
        FFTResult fft = runFFT(&signals[s]);
        float reduction = MAX_SAMPLE_RATE / fft.optimalSampleRate;
        float energySaving = (1.0f - 1.0f / reduction) * 100.0f;

        snprintf(buf, sizeof(buf),
                 "{\"phase\":\"SIGNAL_COMPARISON\",\"signal\":\"%s\",\"dominant_hz\":%.3f,\"adaptive_rate\":%.3f,\"reduction\":%.1f,\"energy_saving_pct\":%.1f}",
                 signals[s].name,
                 fft.dominantFrequency,
                 fft.optimalSampleRate,
                 reduction,
                 energySaving);
        printJSON(buf);

        // One window per signal
        runWindowedAverage(fft.optimalSampleRate, "SIGNAL_WINDOW", &signals[s]);
    }
}

// ======= SETUP & LOOP =======

void setup() {
    Serial0.begin(115200);
    delay(1000); // let serial stabilise

    srand(42); // fixed seed for reproducibility

    Serial0.println("{\"phase\":\"BOOT\",\"max_rate\":1000,\"fft_size\":2048,\"window_secs\":5}");

    // --- Phase 1: Benchmark max sampling rate ---
    benchmarkMaxRate();

    // --- Phase 2: FFT on clean signal ---
    FFTResult fft = runFFT(nullptr);
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"phase\":\"FFT\",\"dominant_hz\":%.3f,\"adaptive_rate\":%.3f,\"reduction\":%.1f,\"energy_saving_pct\":%.1f}",
             fft.dominantFrequency,
             fft.optimalSampleRate,
             MAX_SAMPLE_RATE / fft.optimalSampleRate,
             (1.0f - fft.optimalSampleRate / MAX_SAMPLE_RATE) * 100.0f);
    printJSON(buf);

    // --- Phase 3: Windowed average at max rate (oversampled baseline) ---
    runWindowedAverage(MAX_SAMPLE_RATE, "WINDOW_OVERSAMPLED", nullptr);

    // --- Phase 4: Windowed average at adaptive rate (clean signal) ---
    runWindowedAverage(fft.optimalSampleRate, "WINDOW_ADAPTIVE_CLEAN", nullptr);

    // --- Phase 5: Windowed average noisy unfiltered ---
    {
        int totalSamples = (int) (fft.optimalSampleRate * WINDOW_SECS);
        float *samples = (float *) malloc(totalSamples * sizeof(float));
        char buf2[256];

        Serial0.println("{\"phase\":\"WINDOW_NOISY_START\"}");
        for (int w = 0; w < NUM_WINDOWS; w++) {
            uint64_t e2eStart = esp_timer_get_time();
            float sum = 0.0f;
            for (int i = 0; i < totalSamples; i++) {
                float t = i / fft.optimalSampleRate;
                samples[i] = generateNoisySignal(t, noisyConfig, nullptr);
                sum += samples[i];
            }
            float average = sum / totalSamples;
            float e2eMs = elapsedMs(e2eStart);

            snprintf(buf2, sizeof(buf2),
                     "{\"phase\":\"WINDOW_NOISY\",\"window\":%d,\"average\":%.6f,\"e2e_ms\":%.4f}",
                     w + 1, average, e2eMs);
            printJSON(buf2);
        }
        free(samples);
    }

    // --- Phase 6: Z-score filter ---
    runWindowedFiltered(fft.optimalSampleRate, "WINDOW_ZSCORE", 1);

    // --- Phase 7: Hampel filter ---
    runWindowedFiltered(fft.optimalSampleRate, "WINDOW_HAMPEL", 0);

    // --- Phase 8: Three signals comparison ---
    runSignalsComparison();

    Serial0.println("{\"phase\":\"DONE\"}");
}

void loop() {
    // Everything runs once in setup()
    // Loop kept alive to prevent reset
    delay(10000);
}
