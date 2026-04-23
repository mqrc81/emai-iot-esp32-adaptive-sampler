#pragma once
#include "sampler.h"
#include <mosquitto.h>

struct MQTTConfig
{
    const char* host;
    int port;
    const char* topic;
    const char* clientId;
};

struct MQTTClient
{
    struct mosquitto* mosq;
    MQTTConfig config;
};

MQTTClient mqttConnect(const MQTTConfig& config);
void mqttPublishWindow(MQTTClient& client, const WindowResult& window, float sampleRateHz);
void mqttDisconnect(MQTTClient& client);
