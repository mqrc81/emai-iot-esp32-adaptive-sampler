#include "mqtt_client.h"
#include <stdio.h>
#include <string.h>

MQTTClient mqttConnect(const MQTTConfig& config)
{
    mosquitto_lib_init();
    MQTTClient client;
    client.config = config;
    client.mosq = mosquitto_new(config.clientId, true, nullptr);

    if (!client.mosq)
    {
        fprintf(stderr, "Failed to create mosquitto instance\n");
        return client;
    }

    if (mosquitto_connect(client.mosq, config.host, config.port, 60) != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Failed to connect to broker at %s:%d\n", config.host, config.port);
        mosquitto_destroy(client.mosq);
        client.mosq = nullptr;
        return client;
    }

    printf("Connected to broker at %s:%d\n", config.host, config.port);
    return client;
}

void mqttPublishWindow(MQTTClient& client, const WindowResult& window, float sampleRateHz)
{
    if (!client.mosq)
    {
        fprintf(stderr, "No active MQTT connection\n");
        return;
    }

    char payload[256];
    snprintf(payload, sizeof(payload),
             R"({"average":%.6f,"sample_rate":%.2f,"sample_count":%d,"window_ms":%.3f})",
             window.average,
             sampleRateHz,
             window.sampleCount,
             window.windowDurationMs
    );

    int rc = mosquitto_publish(client.mosq, nullptr, client.config.topic,
                               strlen(payload), payload, 0, false);
    if (rc == MOSQ_ERR_SUCCESS)
        printf("Published: %s\n", payload);
    else
        fprintf(stderr, "Publish failed: %d\n", rc);
}

void mqttDisconnect(MQTTClient& client)
{
    if (!client.mosq) return;
    mosquitto_disconnect(client.mosq);
    mosquitto_destroy(client.mosq);
    mosquitto_lib_cleanup();
    printf("Disconnected from broker\n");
}
