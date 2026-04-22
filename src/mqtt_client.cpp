#include "mqtt_client.h"
#include <mosquitto.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void mqttPublishWindow(const MQTTConfig& config, const WindowResult& window, float sampleRateHz)
{
    mosquitto_lib_init();
    struct mosquitto* mosq = mosquitto_new(config.clientId, true, nullptr);

    if (!mosq)
    {
        fprintf(stderr, "Failed to create mosquitto instance\n");
        return;
    }

    if (mosquitto_connect(mosq, config.host, config.port, 60) != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Failed to connect to broker at %s:%d\n", config.host, config.port);
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return;
    }

    // Build JSON payload
    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"average\":%.6f,\"sample_rate\":%.2f,\"sample_count\":%d,\"window_ms\":%.3f}",
             window.average,
             sampleRateHz,
             window.sampleCount,
             window.windowDurationMs
    );

    int rc = mosquitto_publish(mosq, nullptr, config.topic, strlen(payload), payload, 0, false);
    if (rc == MOSQ_ERR_SUCCESS)
        printf("Published: %s\n", payload);
    else
        fprintf(stderr, "Publish failed: %d\n", rc);

    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}
