#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <RadioLib.h>
#include <time.h>
#include "esp_timer.h"

// ======= TODO: UPDATE WHEN IOT-LAB RESPONDS =======
// Standard Arduino SPI CS = D10, rest are guesses
#define LORA_NSS    10   // SPI CS — educated guess (standard Arduino)
#define LORA_DIO1    2   // unknown — placeholder
#define LORA_RESET   3   // unknown — placeholder
#define LORA_BUSY    4   // unknown — placeholder

// ======= TODO: UPDATE WITH YOUR NETWORK =======
// Leave blank — WiFi scan will reveal available SSIDs
const char *WIFI_SSID = "";
const char *WIFI_PASSWORD = "";

// IoT-Lab MQTT broker
const char *MQTT_HOST = "mqtt4.iot-lab.info";
const int MQTT_PORT = 1883; // try plain first, then 8883 TLS
const char *MQTT_USER = "mschmidt";
const char *MQTT_PASS = ""; // fill in your IoT-Lab password
const char *MQTT_TOPIC = "iotlab/mschmidt/diagnostic";

// ======= GLOBALS =======
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RESET, LORA_BUSY);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
bool wifiOk = false;
bool mqttOk = false;
bool ntpOk = false;

// ======= HELPERS =======

void printJSON(const char *buf) {
    Serial.println(buf);
    // Also publish to MQTT if connected
    if (mqttOk) {
        mqttClient.loop();
        mqttClient.publish(MQTT_TOPIC, buf);
    }
}

void result(const char *test, bool ok, const char *detail = nullptr) {
    char buf[256];
    if (detail)
        snprintf(buf, sizeof(buf),
                 "{\"test\":\"%s\",\"status\":\"%s\",\"detail\":\"%s\"}",
                 test, ok ? "OK" : "FAIL", detail);
    else
        snprintf(buf, sizeof(buf),
                 "{\"test\":\"%s\",\"status\":\"%s\"}",
                 test, ok ? "OK" : "FAIL");
    printJSON(buf);
}

// ======= TEST 1: SERIAL =======
// Trivially passes if you see any output at all
void testSerial() {
    Serial.println("{\"test\":\"SERIAL\",\"status\":\"OK\","
        "\"detail\":\"if you see this serial works\"}");
}

// ======= TEST 2: WIFI SCAN =======
// No credentials needed — reveals available SSIDs
void testWiFiScan() {
    Serial.println("{\"test\":\"WIFI_SCAN\",\"status\":\"RUNNING\"}");
    int n = WiFi.scanNetworks();
    if (n <= 0) {
        result("WIFI_SCAN", false, "no networks found - wifi radio may be disabled");
        return;
    }
    // Print each network as separate JSON line
    for (int i = 0; i < n; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "{\"test\":\"WIFI_NETWORK\",\"index\":%d,\"ssid\":\"%s\","
                 "\"rssi\":%d,\"channel\":%d,\"open\":%s}",
                 i, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                 WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false");
        Serial.println(buf);
    }
    char detail[32];
    snprintf(detail, sizeof(detail), "found %d networks", n);
    result("WIFI_SCAN", true, detail);
}

// ======= TEST 3: WIFI CONNECT =======
// Only runs if SSID is set
void testWiFiConnect() {
    if (strlen(WIFI_SSID) == 0) {
        result("WIFI_CONNECT", false, "no ssid configured - update after scan");
        return;
    }
    Serial.println("{\"test\":\"WIFI_CONNECT\",\"status\":\"RUNNING\"}");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
        delay(500);
        retries++;
    }
    if (WiFi.status() != WL_CONNECTED) {
        result("WIFI_CONNECT", false, "timeout after 10s");
        return;
    }
    wifiOk = true;
    char detail[64];
    snprintf(detail, sizeof(detail), "ip=%s rssi=%ddBm",
             WiFi.localIP().toString().c_str(), WiFi.RSSI());
    result("WIFI_CONNECT", true, detail);
}

// ======= TEST 4: NTP =======
// Syncs real Unix time — needed for E2E latency measurement
void testNTP() {
    if (!wifiOk) {
        result("NTP", false, "skipped - no wifi");
        return;
    }
    Serial.println("{\"test\":\"NTP\",\"status\":\"RUNNING\"}");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = 0;
    int retries = 0;
    while (now < 1000000000L && retries < 20) {
        delay(500);
        now = time(nullptr);
        retries++;
    }
    if (now < 1000000000L) {
        result("NTP", false, "sync timeout");
        return;
    }
    ntpOk = true;
    char detail[64];
    snprintf(detail, sizeof(detail), "unix_time=%lld", (long long) now);
    result("NTP", true, detail);
}

// ======= TEST 5: MQTT =======
// Tests direct WiFi -> MQTT broker connection
void testMQTT() {
    if (!wifiOk) {
        result("MQTT", false, "skipped - no wifi");
        return;
    }
    Serial.println("{\"test\":\"MQTT\",\"status\":\"RUNNING\"}");
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    int retries = 0;
    while (!mqttClient.connected() && retries < 5) {
        if (mqttClient.connect("esp32-diagnostic", MQTT_USER, MQTT_PASS)) {
            mqttOk = true;
        } else {
            delay(1000);
            retries++;
        }
    }
    if (!mqttOk) {
        char detail[32];
        snprintf(detail, sizeof(detail), "rc=%d", mqttClient.state());
        result("MQTT", false, detail);
        return;
    }
    // Publish a test message
    mqttClient.publish(MQTT_TOPIC, "{\"test\":\"MQTT\",\"status\":\"OK\"}");
    result("MQTT", true, "connected and published");
}

// ======= TEST 6: LORA INIT =======
// Tests SX1262 hardware — continues even if it fails
void testLoRa() {
    Serial.println("{\"test\":\"LORA_INIT\",\"status\":\"RUNNING\","
        "\"detail\":\"pins may be wrong - update when IoT-Lab responds\"}");

    // EU868 frequency
    int state = radio.begin(868.0);
    if (state != RADIOLIB_ERR_NONE) {
        char detail[64];
        snprintf(detail, sizeof(detail),
                 "failed code=%d (wrong pins likely)", state);
        result("LORA_INIT", false, detail);
        return;
    }
    result("LORA_INIT", true, "SX1262 responding on current pin config");

    // Test basic TX
    Serial.println("{\"test\":\"LORA_TX\",\"status\":\"RUNNING\"}");
    state = radio.transmit("DIAGNOSTIC_PING");
    if (state != RADIOLIB_ERR_NONE) {
        char detail[32];
        snprintf(detail, sizeof(detail), "tx failed code=%d", state);
        result("LORA_TX", false, detail);
        return;
    }
    result("LORA_TX", true, "packet transmitted");
}

// ======= TEST 7: CONSUMPTION MONITORING NOTE =======
void testConsumptionNote() {
    Serial.println("{\"test\":\"CONSUMPTION_MONITORING\",\"status\":\"INFO\","
        "\"detail\":\"configured via IoT-Lab experiment profile - no firmware change needed\"}");
}

// ======= SETUP & LOOP =======

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(100); // delay for serial bridge to connect

    Serial.println("{\"phase\":\"DIAGNOSTIC_START\","
        "\"board\":\"arduino-nano-esp32\","
        "\"note\":\"all tests independent - failure does not stop subsequent tests\"}");

    testSerial();
    testWiFiScan();
    testWiFiConnect();
    testNTP();
    testMQTT();
    testLoRa();
    testConsumptionNote();

    // Summary
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"phase\":\"DIAGNOSTIC_DONE\","
             "\"wifi\":%s,\"ntp\":%s,\"mqtt\":%s}",
             wifiOk ? "true" : "false",
             ntpOk ? "true" : "false",
             mqttOk ? "true" : "false");
    Serial.println(buf);
}

void loop() {
    // Keep MQTT alive if connected
    if (mqttOk) mqttClient.loop();
    delay(5000);
}
