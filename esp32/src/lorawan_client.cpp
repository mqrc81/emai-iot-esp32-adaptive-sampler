#include "lorawan_client.hpp"
#include "config.hpp"
#include "logger.hpp"
#include <LoRaWAN_ESP32.h>
#include <cstdio>

extern SX1262 radio;

static bool s_joined = false;
static uint32_t s_lastTxMs = 0;

// Minimum interval between uplinks — conservative for EU868 duty cycle
// Only phase summaries are sent, keeping total uplinks ~8 per experiment
static const uint32_t MIN_TX_INTERVAL_MS = 60000; // 60s minimum

// Static instance — lives for program lifetime, no heap allocation needed
static LoRaWANNode s_nodeInstance(&radio, &EU868, 0);
static LoRaWANNode *s_node = &s_nodeInstance;

int cayenneLPPEncode(uint8_t *buf, int bufSize, float value) {
    if (bufSize < 4) return -1;
    int16_t encoded = (int16_t) (value * 100.0f);
    buf[0] = 0x01; // channel 1
    buf[1] = 0x02; // analog input type
    buf[2] = (encoded >> 8) & 0xFF;
    buf[3] = encoded & 0xFF;
    return 4;
}

bool loraInit() {
    // radio pre-initialised by heltec_setup() in setup()
    logMsg("[LORA] LoRaWANNode ready");
    return true;
}

bool loraJoin() {
    logMsg("[LORA] attempting OTAA join...");

    uint8_t appKey[] = TTN_APP_KEY;

    // beginOTAA sets up keys — does not block
    int state = s_node->beginOTAA(TTN_JOIN_EUI, TTN_DEV_EUI, nullptr, appKey);
    if (state != RADIOLIB_ERR_NONE) {
        logFmt("[LORA] OTAA join failed code=%d — continuing without LoRa", state);
        return false;
    }

    // activateOTAA does the actual join — blocks until success or failure
    logMsg("[LORA] activating OTAA...");
    state = s_node->activateOTAA();
    if (state != RADIOLIB_LORAWAN_NEW_SESSION) {
        logFmt("[LORA] OTAA activate failed code=%d — continuing without LoRa", state);
        return false;
    }

    s_joined = true;
    logMsg("[LORA] OTAA join successful");
    return true;
}

bool loraSendWindow(const WindowResult &r) {
    if (!s_joined) {
        logMsg("[LORA] not joined — skipping uplink");
        return false;
    }

    char label[32];
    switch (r.filterType) {
        case 0:
            snprintf(label, sizeof(label), "SIG%d_AVG", r.signalIndex);
            break;
        case 1:
            snprintf(label, sizeof(label), "SIG%d_ZSCORE_p%.0f", r.signalIndex, r.anomalyProb * 100);
            break;
        case 2:
            snprintf(label, sizeof(label), "SIG%d_HAMPEL_p%.0f", r.signalIndex, r.anomalyProb * 100);
            break;
        default:
            logFmt("[LORA] invalid filter type %s – skipping uplink", r.filterType);
            return false;
    }

    uint32_t now = millis();
    if (now - s_lastTxMs < MIN_TX_INTERVAL_MS) {
        logFmt("[LORA] min interval not elapsed — skipping uplink for %s", label);
        return false;
    }

    uint8_t payload[4];
    int len = cayenneLPPEncode(payload, sizeof(payload), r.average);
    if (len < 0) {
        logMsg("[LORA] encoding failed");
        return false;
    }

    logFmt("[LORA] sending uplink for %s value=%.4f", label, r.average);

    // sendReceive blocks ~5-6s for RX1+RX2 windows
    // CommTask calls mqttLoop() immediately after this returns
    int state = s_node->sendReceive(payload, len, 1);
    if (state != RADIOLIB_ERR_NONE) {
        logFmt("[LORA] uplink failed code=%d", state);
        return false;
    }

    s_lastTxMs = millis();
    logFmt("[LORA] uplink sent for %s", label);
    return true;
}
