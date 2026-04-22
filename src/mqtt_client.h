#pragma once
#include "sampler.h"

struct MQTTConfig
{
    const char* host;
    int port;
    const char* topic;
    const char* clientId;
};

void mqttPublishWindow(const MQTTConfig& config, const WindowResult& window, float sampleRateHz);
