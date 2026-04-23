#include "mqtt_client.h"
#include <Arduino.h>

MQTTClient mqttConnect(const MQTTConfig& config)
{
    MQTTClient mc;
    mc.config = config;
    mc.client = new PubSubClient(mc.wifiClient);
    mc.client->setServer(config.host, config.port);

    int retries = 0;
    while (!mc.client->connected() && retries < 5)
    {
        if (mc.client->connect(config.clientId))
        {
            Serial.printf("Connected to broker at %s:%d\n", config.host, config.port);
        }
        else
        {
            Serial.printf("MQTT connect failed, rc=%d, retrying...\n", mc.client->state());
            delay(1000);
            retries++;
        }
    }
    return mc;
}

void mqttPublishWindow(MQTTClient& mc, const WindowResult& window, float sampleRateHz)
{
    if (!mc.client->connected())
    {
        Serial.println("MQTT not connected");
        return;
    }

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"average\":%.6f,\"sample_rate\":%.2f,\"sample_count\":%d,\"window_ms\":%.3f}",
             window.average,
             sampleRateHz,
             window.sampleCount,
             window.windowDurationMs
    );

    mc.client->publish(mc.config.topic, payload);
    Serial.printf("Published: %s\n", payload);
}

void mqttDisconnect(MQTTClient& mc)
{
    if (mc.client)
    {
        mc.client->disconnect();
        delete mc.client;
        mc.client = nullptr;
        Serial.println("Disconnected from broker");
    }
}
