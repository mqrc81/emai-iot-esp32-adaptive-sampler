#include <Arduino.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <Wire.h>
#include <WiFi.h>
#include <ctime>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <heltec_unofficial.h>
#include "config.hpp"
#include "logger.hpp"
#include "signal.hpp"
#include "fft.hpp"
#include "filter.hpp"
#include "power.hpp"
#include "mqtt_client.hpp"
#include "lorawan_client.hpp"

// ======= SERIAL MUTEX =======
SemaphoreHandle_t serialMutex = nullptr;
static SemaphoreHandle_t s_phaseMutex = nullptr;

// ======= QUEUES =======
static QueueHandle_t s_sampleQueue = nullptr; // SamplerTask → FFTTask
static QueueHandle_t s_rateQueue = nullptr; // FFTTask → SamplerTask
static QueueHandle_t s_windowQueue = nullptr; // SamplerTask → FilterTask
static QueueHandle_t s_resultQueue = nullptr; // FilterTask → CommTask
static QueueHandle_t s_powerQueue = nullptr; // MonitorTask → CommTask

// ======= SHARED EXPERIMENT STATE =======
static char s_currentPhase[32] = "INIT";
static std::atomic<bool> s_experimentDone{false};

// ======= STRUCTS =======
struct SampleBuffer {
    float *data;
    int size;
    float sampleRate;
};

struct WindowBuffer {
    float *data;
    int *groundTruth;
    int size;
    float sampleRate;
    float adaptiveRate;
    int signalIndex;
    int filterType; // 0=none, 1=zscore, 2=hampel
    float anomalyProb;
    int windowIndex;
};

// ======= HELPERS =======
static int64_t nowUs() {
    timeval tv{};
    gettimeofday(&tv, nullptr);
    return (int64_t) tv.tv_sec * 1000000LL + tv.tv_usec;
}

static void waitForResultsQueueDrain() {
    while (uxQueueMessagesWaiting(s_resultQueue) > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(200)); // extra buffer for in-flight MQTT publish
}

static void setPhase(const char *phase) {
    waitForResultsQueueDrain(); // prevent mislabeled phases
    if (xSemaphoreTake(s_phaseMutex, pdMS_TO_TICKS(100))) {
        strncpy(s_currentPhase, phase, sizeof(s_currentPhase) - 1);
        s_currentPhase[sizeof(s_currentPhase) - 1] = '\0';
        xSemaphoreGive(s_phaseMutex);
    }
    logFmt("[PHASE] %s", phase);
    oledClear();
    oledStatus(0, "Phase:");
    oledStatus(1, phase);
}

// ======= WIFI =======
static bool connectWiFi() {
    logFmt("[WIFI] connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        retries++;
    }
    if (WiFi.status() != WL_CONNECTED) {
        logMsg("[WIFI] failed — continuing without MQTT");
        return false;
    }
    logFmt("[WIFI] connected ip=%s", WiFi.localIP().toString().c_str());
    return true;
}

static void syncNTP() {
    if (WiFi.status() != WL_CONNECTED) return;
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    logMsg("[NTP] syncing...");
    time_t now = 0;
    int retries = 0;
    while (now < 1000000000L && retries < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        now = time(nullptr);
        retries++;
    }
    if (now > 1000000000L)
        logFmt("[NTP] synced unix=%lld", (long long) now);
    else
        logMsg("[NTP] sync failed");
}

// ======= MONITOR TASK =======
static void monitorTask(void *param) {
    for (;;) {
        if (s_experimentDone.load()) vTaskDelete(nullptr);
        char phase[32];
        if (xSemaphoreTake(s_phaseMutex, pdMS_TO_TICKS(100))) {
            strncpy(phase, s_currentPhase, sizeof(phase));
            xSemaphoreGive(s_phaseMutex);
        }
        PowerReading r = powerRead(phase);
        oledStatus(4, "%.1fmA %.2fV", r.currentMa, r.voltageV);
        xQueueOverwrite(s_powerQueue, &r);
        vTaskDelay(pdMS_TO_TICKS(INA219_POLL_MS));
    }
}

// ======= COMM TASK =======
static void commTask(void *param) {
    for (;;) {
        if (s_experimentDone.load()) {
            mqttLoop();
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        WindowResult r{};
        // Block up to 5s waiting for result — then loop for keepalive
        if (xQueueReceive(s_resultQueue, &r, pdMS_TO_TICKS(5000))) {
            PowerReading power = {};
            xQueuePeek(s_powerQueue, &power, pdMS_TO_TICKS(100));

            mqttEnsureConnected(MQTT_USER, MQTT_PASS);
            mqttPublishWindow(r, power);

            // Send windowed average via LoRaWAN — only for last window of each phase to respect duty cycle
            if (r.windowIndex == NUM_WINDOWS - 1 && r.signalIndex >= 0) {
                loraSendWindow(r);
            }
        }

        mqttEnsureConnected(MQTT_USER, MQTT_PASS);
        mqttLoop();
    }
}

// ======= FILTER TASK =======
static void filterTask(void *param) {
    for (;;) {
        WindowBuffer wb{};
        if (xQueueReceive(s_windowQueue, &wb, portMAX_DELAY) != pdTRUE) continue;

        uint64_t computeStart = esp_timer_get_time();

        int half = FILTER_WINDOW / 2;
        int tp = 0, fp = 0, fn = 0, tn = 0;
        float totalError = 0.0f;
        float sum = 0.0f;
        int counted = 0;

        for (int i = 0; i < wb.size; i++) {
            float cleaned;
            int flagged = 0;

            if (i >= half && i < wb.size - half) {
                const float *window = &wb.data[i - half];
                FilterResult fr{};

                if (wb.filterType == 1)
                    fr = zscoreFilter(window, FILTER_WINDOW, half, ZSCORE_THRESH);
                else if (wb.filterType == 2)
                    fr = hampelFilter(window, FILTER_WINDOW, half, HAMPEL_THRESH);
                else
                    fr = {0, wb.data[i]};

                cleaned = fr.cleaned;
                flagged = fr.flagged;

                // Confusion matrix (only meaningful for noisy signal)
                if (wb.groundTruth) {
                    int gt = wb.groundTruth[i];
                    if (gt == 1 && flagged == 1) tp++;
                    if (gt == 0 && flagged == 1) fp++;
                    if (gt == 1 && flagged == 0) fn++;
                    if (gt == 0 && flagged == 0) tn++;
                }

                // Error vs clean signal
                const SignalDef &sigDef = (wb.signalIndex >= 0 && wb.signalIndex < 3)
                                              ? *ALL_SIGNALS[wb.signalIndex]
                                              : SIGNAL_1;
                float trueVal = generateSignal(i / wb.adaptiveRate, sigDef);
                totalError += fabsf(cleaned - trueVal);
                counted++;
            } else {
                cleaned = wb.data[i];
            }
            sum += cleaned;
        }

        float computeMs = (esp_timer_get_time() - computeStart) / 1000.0f;
        float average = sum / wb.size;
        int evaluated = counted > 0 ? counted : 1;

        WindowResult r{};
        r.windowIndex = wb.windowIndex;
        r.average = average;
        r.adaptiveRate = wb.adaptiveRate;
        r.sampleCount = wb.size;
        r.computeMs = computeMs;
        r.timestampUs = nowUs();
        r.signalIndex = wb.signalIndex;
        r.filterType = wb.filterType;
        r.anomalyProb = wb.anomalyProb;
        r.tpr = (float) tp / (tp + fn + 1e-6f);
        r.fpr = (float) fp / (fp + tn + 1e-6f);
        r.meanError = totalError / evaluated;
        r.tp = tp;
        r.fp = fp;
        r.fn = fn;
        r.tn = tn;
        r.bytes = r.sampleCount * sizeof(float);

        xQueueSend(s_resultQueue, &r, portMAX_DELAY);

        // Free buffers allocated by SamplerTask
        free(wb.data);
        if (wb.groundTruth) free(wb.groundTruth);
    }
}

// ======= FFT TASK =======
static void fftTask(void *param) {
    for (;;) {
        SampleBuffer sb{};
        if (xQueueReceive(s_sampleQueue, &sb, portMAX_DELAY) != pdTRUE) continue;

        setPhase("FFT");
        FFTResult fft = computeFFT(sb.data, sb.size, sb.sampleRate);
        free(sb.data);

        if (fft.dominantFrequency == 0.0f) {
            logMsg("[FFT] failed — keeping current rate");
            continue;
        }

        logFmt("[FFT] dominant=%.2fHz adaptive=%.2fHz reduction=%.1fx",
               fft.dominantFrequency,
               fft.optimalSampleRate,
               sb.sampleRate / fft.optimalSampleRate);

        // Send adaptive rate back to SamplerTask
        xQueueSend(s_rateQueue, &fft.optimalSampleRate, portMAX_DELAY);
    }
}

// ======= SAMPLER TASK =======
// Runs the full experiment sequentially
static void samplerTask(void *param) {
    vTaskDelay(pdMS_TO_TICKS(3000));

    float adaptiveRate = MAX_SAMPLE_RATE;

    // ===== PHASE 1: BENCHMARK MAX RATE =====
    setPhase("BENCHMARK");
    powerResetEnergy();

    uint64_t benchStart = esp_timer_get_time();
    volatile float sink = 0.0f;
    for (int i = 0; i < BENCHMARK_SAMPLES; i++) {
        float t = i / MAX_SAMPLE_RATE;
        sink = generateSignal(t, SIGNAL_1);
    }
    float benchElapsedS = (esp_timer_get_time() - benchStart) / 1e6f;
    float achievedRateHz = BENCHMARK_SAMPLES / benchElapsedS;
    logFmt("[BENCH] achieved=%.2fHz elapsed=%.4fs", achievedRateHz, benchElapsedS);

    // Publish benchmark result
    WindowResult benchResult = {};
    benchResult.adaptiveRate = achievedRateHz;
    benchResult.computeMs = benchElapsedS * 1000.0f;
    benchResult.timestampUs = nowUs();
    xQueueSend(s_resultQueue, &benchResult, portMAX_DELAY);

    // ===== PHASE 2: FFT → ADAPTIVE RATE =====
    setPhase("FFT_COLLECT");

    float *buf = (float *) malloc(FFT_SIZE * sizeof(float));
    if (!buf) {
        logMsg("[SAMPLER] FFT malloc failed");
        vTaskDelete(nullptr);
    }

    for (int i = 0; i < FFT_SIZE; i++) {
        buf[i] = generateSignal(i / MAX_SAMPLE_RATE, SIGNAL_1);
        vTaskDelay(pdMS_TO_TICKS(1000.0f / MAX_SAMPLE_RATE)); // 1ms at 1000Hz
    }

    SampleBuffer sb = {buf, FFT_SIZE, MAX_SAMPLE_RATE};
    xQueueSend(s_sampleQueue, &sb, portMAX_DELAY);

    // Wait for adaptive rate from FFTTask
    xQueueReceive(s_rateQueue, &adaptiveRate, portMAX_DELAY);
    logFmt("[SAMPLER] adaptive rate set to %.2fHz", adaptiveRate);

    // ===== PHASES 3 & 6: THREE SIGNALS — CLEAN WINDOWED AVERAGE =====
    for (int sigIdx = 0; sigIdx < 3; sigIdx++) {
        const SignalDef &sig = *ALL_SIGNALS[sigIdx];
        powerResetEnergy();

        char fftLabel[32];
        snprintf(fftLabel, sizeof(fftLabel), "SIG%d_FFT", sigIdx);
        setPhase(fftLabel); // e.g. SIG0_FFT

        // Run FFT for this signal to get its adaptive rate
        float *fftBuf = (float *) malloc(FFT_SIZE * sizeof(float));
        if (!fftBuf) {
            logMsg("[SAMPLER] FFT malloc failed");
            continue;
        }
        for (int i = 0; i < FFT_SIZE; i++) {
            fftBuf[i] = generateSignal(i / MAX_SAMPLE_RATE, sig);
            vTaskDelay(pdMS_TO_TICKS(1000.0f / MAX_SAMPLE_RATE)); // 1ms at 1000Hz
        }
        SampleBuffer sb = {fftBuf, FFT_SIZE, MAX_SAMPLE_RATE};
        xQueueSend(s_sampleQueue, &sb, portMAX_DELAY);

        float sigAdaptiveRate = MAX_SAMPLE_RATE;
        xQueueReceive(s_rateQueue, &sigAdaptiveRate, portMAX_DELAY);
        logFmt("[SAMPLER] signal %d adaptive=%.2fHz", sigIdx, sigAdaptiveRate);

        char windowLabel[32];
        snprintf(windowLabel, sizeof(windowLabel), "SIG%d_WINDOW", sigIdx);
        setPhase(windowLabel); // e.g. SIG0_WINDOW

        int totalSamples = (int) (sigAdaptiveRate * WINDOW_SECS);

        for (int w = 0; w < NUM_WINDOWS; w++) {
            float *winBuf = (float *) malloc(totalSamples * sizeof(float));
            if (!winBuf) {
                logMsg("[SAMPLER] window malloc failed");
                continue;
            }

            for (int i = 0; i < totalSamples; i++) {
                winBuf[i] = generateSignal(i / sigAdaptiveRate, sig);
                vTaskDelay(pdMS_TO_TICKS(1000.0f / sigAdaptiveRate)); // e.g. ~102ms at 9.77Hz
            }

            WindowBuffer wb = {};
            wb.data = winBuf;
            wb.groundTruth = nullptr;
            wb.size = totalSamples;
            wb.sampleRate = MAX_SAMPLE_RATE;
            wb.adaptiveRate = sigAdaptiveRate;
            wb.signalIndex = sigIdx;
            wb.filterType = 0; // no filter
            wb.anomalyProb = 0.0f;
            wb.windowIndex = w;

            xQueueSend(s_windowQueue, &wb, portMAX_DELAY);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Log signal timeseries data
    if (LOG_SIGNAL_TIMESERIES) {
        logMsg("[SAMPLER] ========== signal 1 timeseries ==========");
        logMsg("t,clean,noisy,anomaly");
        for (int i = 0; i < MAX_SAMPLE_RATE; i++) {
            float t = i / MAX_SAMPLE_RATE;
            int isAnomaly;
            SignalConfig cfg = DEFAULT_NOISY_CONFIG;
            cfg.anomalyProb = 0.02f; // p=2% matches simulation plot
            float noisy = generateNoisySignal(t, cfg, &isAnomaly);
            float clean = generateSignal(t, SIGNAL_1);
            logFmt("%.4f,%.4f,%.4f,%d", t, clean, noisy, isAnomaly);
        }
        logMsg("[SAMPLER] =========================================");
    }

    // ===== PHASE 4: NOISY SIGNAL — MULTIPLE INJECTION RATES =====
    float anomalyProbs[] = {0.01f, 0.05f, 0.10f};
    int filterTypes[] = {1, 2}; // 1=zscore, 2=hampel

    for (float prob: anomalyProbs) {
        SignalConfig noisyCfg = DEFAULT_NOISY_CONFIG;
        noisyCfg.anomalyProb = prob;

        char phaseLabel[32];
        snprintf(phaseLabel, sizeof(phaseLabel), "NOISY_p%.0f", prob * 100);
        setPhase(phaseLabel);
        powerResetEnergy();

        // FFT comparison: pre- vs post-filter on 128-sample noisy buffer
        // 128 is the nearest power of 2 that gives adequate frequency resolution
        // at the adaptive rate (~9.77 Hz × 13 s). The window-average buffers
        // (totalSamples ≈ 48) are too small and not a power of 2, so we collect
        // a dedicated buffer here.
        {
            static constexpr int FFT_NOISY_SIZE = 128;
            float *noisyFftBuf = (float *) malloc(FFT_NOISY_SIZE * sizeof(float));
            float *filteredBuf = (float *) malloc(FFT_NOISY_SIZE * sizeof(float));

            if (!noisyFftBuf || !filteredBuf) {
                logMsg("[SAMPLER] noisy FFT malloc failed");
                free(noisyFftBuf);
                free(filteredBuf);
            } else {
                // Generate 128 noisy samples (ground truth discarded here)
                for (int i = 0; i < FFT_NOISY_SIZE; i++) {
                    noisyFftBuf[i] = generateNoisySignal(i / adaptiveRate, noisyCfg, nullptr);
                }

                // Pre-filter FFT
                FFTResult preFft = computeFFT(noisyFftBuf, FFT_NOISY_SIZE, adaptiveRate);
                logFmt("[FFT_NOISY] p=%.0f%% pre-filter  dominant=%.2fHz optRate=%.2fHz",
                       prob * 100, preFft.dominantFrequency, preFft.optimalSampleRate);

                // Filter with Z-score (representative; Hampel would be symmetric)
                int half = FILTER_WINDOW / 2;
                for (int i = 0; i < FFT_NOISY_SIZE; i++) {
                    if (i >= half && i < FFT_NOISY_SIZE - half) {
                        FilterResult fr = zscoreFilter(&noisyFftBuf[i - half], FILTER_WINDOW, half, ZSCORE_THRESH);
                        filteredBuf[i] = fr.cleaned;
                    } else {
                        filteredBuf[i] = noisyFftBuf[i];
                    }
                }

                // Post-filter FFT
                FFTResult postFft = computeFFT(filteredBuf, FFT_NOISY_SIZE, adaptiveRate);
                logFmt("[FFT_NOISY] p=%.0f%% post-filter dominant=%.2fHz optRate=%.2fHz",
                       prob * 100, postFft.dominantFrequency, postFft.optimalSampleRate);

                logFmt("[FFT_NOISY] p=%.0f%% rate delta=%.2fHz (filtering saves %.2fHz over-sampling)",
                       prob * 100,
                       preFft.optimalSampleRate - postFft.optimalSampleRate,
                       preFft.optimalSampleRate - postFft.optimalSampleRate);

                free(noisyFftBuf);
                free(filteredBuf);
            }
        }

        int totalSamples = (int) (adaptiveRate * WINDOW_SECS);

        for (int filterType: filterTypes) {
            for (int w = 0; w < NUM_WINDOWS; w++) {
                float *winBuf = (float *) malloc(totalSamples * sizeof(float));
                int *gt = (int *) malloc(totalSamples * sizeof(int));
                if (!winBuf || !gt) {
                    free(winBuf);
                    free(gt);
                    logMsg("[SAMPLER] noisy malloc failed");
                    continue;
                }

                for (int i = 0; i < totalSamples; i++) {
                    winBuf[i] = generateNoisySignal(i / adaptiveRate, noisyCfg, &gt[i]);
                    vTaskDelay(pdMS_TO_TICKS(1000.0f / adaptiveRate));
                }

                WindowBuffer wb = {};
                wb.data = winBuf;
                wb.groundTruth = gt;
                wb.size = totalSamples;
                wb.sampleRate = MAX_SAMPLE_RATE;
                wb.adaptiveRate = adaptiveRate;
                wb.signalIndex = 0; // Signal 1 only for noisy
                wb.filterType = filterType;
                wb.anomalyProb = prob;
                wb.windowIndex = w;

                xQueueSend(s_windowQueue, &wb, portMAX_DELAY);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // ===== PHASE 5: WINDOW SIZE TRADE-OFF =====
    setPhase("WINDOW_TRADEOFF");
    powerResetEnergy();
    {
        int windowSizes[] = {5, 11, 21, 51, 101, 201};
        int nSizes = 6;
        float prob = 0.05f;

        SignalConfig noisyCfg = DEFAULT_NOISY_CONFIG;
        noisyCfg.anomalyProb = prob;

        int totalSamples = (int) (adaptiveRate * TRADEOFF_WINDOW_SECS);
        float *samples = (float *) malloc(totalSamples * sizeof(float));
        int *gt = (int *) malloc(totalSamples * sizeof(int));

        if (!samples || !gt) {
            logMsg("[SAMPLER] tradeoff malloc failed");
            free(samples);
            free(gt);
            goto done;
        }

        for (int i = 0; i < totalSamples; i++) {
            samples[i] = generateNoisySignal(i / adaptiveRate, noisyCfg, &gt[i]);
            vTaskDelay(pdMS_TO_TICKS(1000.0f / adaptiveRate));
        }

        for (int wi = 0; wi < nSizes; wi++) {
            int W = windowSizes[wi];
            int half = W / 2;

            // Z-score
            {
                int tp = 0, fp = 0, fn = 0, tn = 0;
                float totalError = 0.0f;
                uint64_t start = esp_timer_get_time();

                for (int i = half; i < totalSamples - half; i++) {
                    FilterResult fr = zscoreFilter(&samples[i - half], W, half, ZSCORE_THRESH);
                    int g = gt[i];
                    if (g == 1 && fr.flagged == 1) tp++;
                    if (g == 0 && fr.flagged == 1) fp++;
                    if (g == 1 && fr.flagged == 0) fn++;
                    if (g == 0 && fr.flagged == 0) tn++;
                    float trueVal = generateSignal(i / adaptiveRate, SIGNAL_1);
                    totalError += fabsf(fr.cleaned - trueVal);
                }

                float timeMs = (esp_timer_get_time() - start) / 1000.0f;
                int evaluated = totalSamples - 2 * half;
                float tpr = (float) tp / (tp + fn + 1e-6f);
                float fpr = (float) fp / (fp + tn + 1e-6f);
                float meanErr = totalError / evaluated;

                WindowResult r = {};
                r.windowIndex = wi;
                r.adaptiveRate = (float) W; // reuse field for window size W
                r.computeMs = timeMs;
                r.timestampUs = nowUs();
                r.signalIndex = -1; // sentinel for trade-off phase
                r.filterType = 1;
                r.anomalyProb = prob;
                r.tpr = tpr;
                r.fpr = fpr;
                r.meanError = meanErr;
                r.sampleCount = evaluated;
                r.tp = tp;
                r.fp = fp;
                r.fn = fn;
                r.tn = tn;
                r.bytes = W * (int) sizeof(float);
                xQueueSend(s_resultQueue, &r, portMAX_DELAY);
            }

            // Hampel
            {
                int tp = 0, fp = 0, fn = 0, tn = 0;
                float totalError = 0.0f;
                uint64_t start = esp_timer_get_time();

                for (int i = half; i < totalSamples - half; i++) {
                    FilterResult fr = hampelFilter(&samples[i - half], W, half, HAMPEL_THRESH);
                    int g = gt[i];
                    if (g == 1 && fr.flagged == 1) tp++;
                    if (g == 0 && fr.flagged == 1) fp++;
                    if (g == 1 && fr.flagged == 0) fn++;
                    if (g == 0 && fr.flagged == 0) tn++;
                    float trueVal = generateSignal(i / adaptiveRate, SIGNAL_1);
                    totalError += fabsf(fr.cleaned - trueVal);
                }

                float timeMs = (esp_timer_get_time() - start) / 1000.0f;
                int evaluated = totalSamples - 2 * half;
                float tpr = (float) tp / (tp + fn + 1e-6f);
                float fpr = (float) fp / (fp + tn + 1e-6f);
                float meanErr = totalError / evaluated;

                WindowResult r = {};
                r.windowIndex = wi;
                r.adaptiveRate = (float) W; // reuse field for window size W
                r.computeMs = timeMs;
                r.timestampUs = nowUs();
                r.signalIndex = -1; // sentinel for trade-off phase
                r.filterType = 2;
                r.anomalyProb = prob;
                r.tpr = tpr;
                r.fpr = fpr;
                r.meanError = meanErr;
                r.sampleCount = evaluated;
                r.tp = tp;
                r.fp = fp;
                r.fn = fn;
                r.tn = tn;
                r.bytes = W * (int) sizeof(float);
                xQueueSend(s_resultQueue, &r, portMAX_DELAY);
            }
        }

        free(samples);
        free(gt);
    }

done:
    waitForResultsQueueDrain();
    s_experimentDone.store(true);
    logMsg("[SAMPLER] experiment complete");
    vTaskDelete(nullptr);
}

// ======= SETUP =======
void setup() {
    heltec_setup(); // initialises radio, must be first
    delay(1000);
    heltec_ve(true);
    delay(5000);

    serialMutex = xSemaphoreCreateMutex();
    s_phaseMutex = xSemaphoreCreateMutex();

    oledClear();
    oledStatus(0, "BOOTING...");
    logMsg("[BOOT] starting...");

    // I2C for INA219 — using Wire1 so it doesn't conflict with internal display wiring
    Wire1.begin(INA219_SDA, INA219_SCL);
    bool powerOk = powerInit();
    if (!powerOk) logMsg("[BOOT] INA219 not found — continuing without power measurement");
    oledClear();
    oledStatus(0, powerOk ? "Power OK" : "Power FAIL");

    // WiFi + NTP
    bool wifiOK = connectWiFi();
    oledStatus(1, wifiOK ? "WiFi OK" : "WiFi FAIL");
    syncNTP();

    // MQTT
    mqttInit(MQTT_HOST, MQTT_PORT, MQTT_CLIENT);
    bool mqttOK = mqttEnsureConnected(MQTT_USER, MQTT_PASS);
    oledStatus(2, mqttOK ? "MQTT OK" : "MQTT FAIL");

    // LoRa
    loraInit();
    bool loraOK = loraJoin(); // blocks until joined or fails
    oledStatus(3, loraOK ? "LoRa OK" : "LoRa FAIL");

    // Create queues
    s_sampleQueue = xQueueCreate(1, sizeof(SampleBuffer));
    s_rateQueue = xQueueCreate(1, sizeof(float));
    s_windowQueue = xQueueCreate(2, sizeof(WindowBuffer));
    s_resultQueue = xQueueCreate(16, sizeof(WindowResult));
    s_powerQueue = xQueueCreate(1, sizeof(PowerReading));

    // Create tasks
    xTaskCreate(monitorTask, "Monitor", STACK_MONITOR, nullptr, PRIO_MONITOR, nullptr);
    xTaskCreate(commTask, "Comm", STACK_COMM, nullptr, PRIO_COMM, nullptr);
    xTaskCreate(filterTask, "Filter", STACK_FILTER, nullptr, PRIO_FILTER, nullptr);
    xTaskCreate(fftTask, "FFT", STACK_FFT, nullptr, PRIO_FFT, nullptr);
    xTaskCreate(samplerTask, "Sampler", STACK_SAMPLER, nullptr, PRIO_SAMPLER, nullptr);

    logMsg("[BOOT] all tasks started");
}

void loop() {
    heltec_loop();
    vTaskDelay(pdMS_TO_TICKS(100));
}
