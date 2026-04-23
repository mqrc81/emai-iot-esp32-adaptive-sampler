#pragma once
#include <PubSubClient.h>
#include <WiFi.h>
#include "sampler.h"

struct MQTTConfig
{
    const char* host;
    int port;
    const char* topic;
    const char* clientId;
};

struct MQTTClient
{
    PubSubClient* client;
    WiFiClient wifiClient;
    struct MQTTConfig config;
};

struct MQTTClient mqttConnect(MQTTConfig& config);
void mqttPublishWindow(MQTTClient& client, WindowResult& window, float sampleRateHz);
void mqttDisconnect(MQTTClient& client);
