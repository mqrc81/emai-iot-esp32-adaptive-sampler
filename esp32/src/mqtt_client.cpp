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

void mqttPublishRaw(const char *payload, int len, int bufSize) {
    if (!s_mqtt.connected()) return;

    if (len >= bufSize) {
        logFmt("[MQTT] payload truncated — not publishing (%d >= %d)", len, bufSize);
        return;
    }

    if (!s_mqtt.publish(MQTT_TOPIC, payload))
        logMsg("[MQTT] publish failed");
}

void mqttPublishWindow(const WindowResult &r, const PowerReading &power) {
    char payload[512];
    int len = snprintf(payload, sizeof(payload),
                       "{\"phase\":\"%s\","
                       "\"idx\":%d,\"average\":%.6f,\"adaptive_rate\":%.2f,\"sample_count\":%d,"
                       "\"compute_ms\":%.4f,\"timestamp_us\":%lld,"
                       "\"signal\":%d,\"filter\":%d,\"anomaly_prob\":%.2f,"
                       "\"tpr\":%.4f,\"fpr\":%.4f,\"mean_err\":%.6f,"
                       "\"tp\":%d,\"fp\":%d,\"fn\":%d,\"tn\":%d,"
                       "\"bytes\":%d,"
                       "\"current_ma\":%.2f,\"voltage_v\":%.3f,"
                       "\"power_mw\":%.2f,\"energy_mj\":%.4f}",
                       power.phase,
                       r.windowIndex, r.average, r.adaptiveRate, r.sampleCount,
                       r.computeMs, r.timestampUs,
                       r.signalIndex, r.filterType, r.anomalyProb,
                       r.tpr, r.fpr, r.meanError,
                       r.tp, r.fp, r.fn, r.tn,
                       r.bytes,
                       power.currentMa, power.voltageV,
                       power.powerMw, power.energyMj);

    mqttPublishRaw(payload, len, sizeof(payload));
}

void mqttPublishFFTComparison(const WindowResult &r) {
    // TODO
    char payload[1] = "";
    int len = 1;
    mqttPublishRaw(payload, len, sizeof(payload));
}

void mqttLoop() {
    s_mqtt.loop();
}
