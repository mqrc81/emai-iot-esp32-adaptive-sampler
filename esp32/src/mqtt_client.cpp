#include "mqtt_client.hpp"
#include "config.hpp"
#include "logger.hpp"
#include <cstdio>
#include <freertos/semphr.h>

// Only CommTask calls publish functions — no mutex needed
// MonitorTask sends PowerReadings via queue to CommTask
static WiFiClient s_wifiClient;
static PubSubClient s_mqtt(s_wifiClient);
static const char *s_host = nullptr;
static int s_port = 0;
static const char *s_clientId = nullptr;

bool mqttInit(const char *host, int port, const char *clientId) {
    s_host = host;
    s_port = port;
    s_clientId = clientId;
    s_mqtt.setServer(host, port);
    s_mqtt.setBufferSize(512);
    logFmt("[MQTT] configured %s:%d", host, port);
    return true;
}

bool mqttEnsureConnected(const char *user, const char *password) {
    if (s_mqtt.connected()) return true;
    logMsg("[MQTT] connecting...");
    int retries = 0;
    while (!s_mqtt.connected() && retries < 5) {
        if (s_mqtt.connect(s_clientId, user, password)) {
            logMsg("[MQTT] connected");
            return true;
        }
        logFmt("[MQTT] failed rc=%d retrying...", s_mqtt.state());
        vTaskDelay(pdMS_TO_TICKS(1000));
        retries++;
    }
    logMsg("[MQTT] failed to connect");
    return false;
}

void mqttPublishWindow(const WindowResult &r, const PowerReading &power) {
    if (!s_mqtt.connected()) return;

    char payload[512];
    int len = snprintf(payload, sizeof(payload),
                       "{\"average\":%.6f,\"adaptive_rate\":%.2f,\"sample_count\":%d,"
                       "\"compute_ms\":%.4f,\"timestamp_us\":%lld,"
                       "\"signal\":%d,\"filter\":%d,\"anomaly_prob\":%.2f,"
                       "\"tpr\":%.4f,\"fpr\":%.4f,\"mean_error\":%.6f,"
                       "\"tp\":%d,\"fp\":%d,\"fn\":%d,\"tn\":%d,"
                       "\"current_ma\":%.2f,\"voltage_v\":%.3f,"
                       "\"power_mw\":%.2f,\"energy_mj\":%.4f,\"phase\":\"%s\"}",
                       r.average, r.adaptiveRate, r.sampleCount,
                       r.computeMs, r.timestampUs,
                       r.signalIndex, r.filterType, r.anomalyProb,
                       r.tpr, r.fpr, r.meanError,
                       r.tp, r.fp, r.fn, r.tn,
                       power.currentMa, power.voltageV,
                       power.powerMw, power.energyMj, power.phase);

    if (len >= (int) sizeof(payload)) {
        logMsg("[MQTT] payload truncated — not publishing");
        return;
    }

    if (!s_mqtt.publish(MQTT_TOPIC, payload))
        logMsg("[MQTT] publish failed");
}

void mqttPublishPower(const PowerReading &power) {
    if (!s_mqtt.connected()) return;

    char payload[256];
    int len = snprintf(payload, sizeof(payload),
                       "{\"current_ma\":%.2f,\"voltage_v\":%.3f,"
                       "\"power_mw\":%.2f,\"energy_mj\":%.4f,\"phase\":\"%s\"}",
                       power.currentMa, power.voltageV,
                       power.powerMw, power.energyMj, power.phase);

    if (len >= (int) sizeof(payload)) {
        logMsg("[MQTT] power payload truncated");
        return;
    }

    s_mqtt.publish(MQTT_TOPIC "/power", payload);
}

void mqttPublishRaw(const char *topic, const char *payload) {
    if (!s_mqtt.connected()) return;
    s_mqtt.publish(topic, payload);
}

void mqttLoop() {
    s_mqtt.loop();
}
