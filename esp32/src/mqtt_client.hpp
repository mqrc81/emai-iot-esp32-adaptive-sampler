#pragma once
#include <Arduino.h>
#include <cstdint>
#include <WiFi.h>
#include <PubSubClient.h>
#include "power.hpp"

struct WindowResult {
    int windowIndex;
    float average;
    float adaptiveRate; // in trade-off phase reused for window size W
    int sampleCount;
    float computeMs;
    int64_t timestampUs;
    int signalIndex; // -1 = sentinel for trade-off phase
    int filterType; // 0=none, 1=zscore, 2=hampel
    float anomalyProb;
    float tpr;
    float fpr;
    float meanError;
    int tp, fp, fn, tn;
    int bytes; // compare against oversampled bytes: (MAX_SAMPLE_RATE * WINDOW_SECS) * sizeof(float)
};

bool mqttInit(const char *host, int port, const char *clientId);

bool mqttEnsureConnected(const char *user, const char *password);

void mqttPublishWindow(const WindowResult &result, const PowerReading &power);

void mqttPublishFFTComparison(const WindowResult &result);

void mqttLoop();
