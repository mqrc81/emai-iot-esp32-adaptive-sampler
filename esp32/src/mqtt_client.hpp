#pragma once
#include <Arduino.h>
#include <cstdint>
#include <WiFi.h>
#include <PubSubClient.h>
#include "power.hpp"

struct WindowResult {
    float average;
    float adaptiveRate;
    int sampleCount;
    float computeMs;
    int64_t timestampUs;
    int signalIndex;
    int filterType; // 0=none, 1=zscore, 2=hampel
    float anomalyProb;
    float tpr;
    float fpr;
    float meanError;
    int tp, fp, fn, tn;
    int windowIndex;
    int bytesAdaptive;
    int bytesOversampled;
};

bool mqttInit(const char *host, int port, const char *clientId);

bool mqttEnsureConnected(const char *user, const char *password);

void mqttPublishWindow(const WindowResult &result, const PowerReading &power);

void mqttPublishPower(const PowerReading &power);

void mqttPublishRaw(const char *topic, const char *payload);

void mqttLoop();
