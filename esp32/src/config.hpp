#pragma once

// ======= WIFI =======
#define WIFI_SSID     "your_ssid"
#define WIFI_PASSWORD "your_password"

// ======= MQTT =======
#define MQTT_HOST     "192.168.1.127"  // Mac's IP (ipconfig getifaddr en0)
#define MQTT_PORT     1883
#define MQTT_TOPIC    "iot/window"
#define MQTT_CLIENT   "heltec-adaptive-sampler"

// ======= TTN / LORAWAN =======
#define TTN_DEV_EUI   "0000000000000000"
#define TTN_APP_EUI   "0000000000000000"
#define TTN_APP_KEY   "00000000000000000000000000000000"
#define LORA_FREQ     868.0  // EU868

// ======= SX1262 PINS (Heltec WiFi LoRa 32 V3.2) =======
#define LORA_NSS      8
#define LORA_DIO1     14
#define LORA_RESET    12
#define LORA_BUSY     13
#define LORA_SCK      9
#define LORA_MOSI     10
#define LORA_MISO     11

// ======= INA219 I2C PINS =======
#define INA219_SDA    17
#define INA219_SCL    18

// ======= EXPERIMENT =======
#define MAX_SAMPLE_RATE     1000.0f
#define FFT_SIZE            2048
#define WINDOW_SECS         5.0f
#define NUM_WINDOWS         5
#define FILTER_WINDOW       21
#define ZSCORE_THRESH       3.0f
#define HAMPEL_THRESH       3.0f
#define BENCHMARK_SAMPLES   10000

// ======= TASK STACK SIZES =======
#define STACK_SAMPLER   12288   // 12KB - holds 2048 float buffer
#define STACK_FFT       24576   // 24KB - holds two 2048 double buffers
#define STACK_FILTER    8192    // 8KB
#define STACK_COMM      8192    // 8KB
#define STACK_MONITOR   4096    // 4KB

// ======= TASK PRIORITIES =======
#define PRIO_SAMPLER    3
#define PRIO_FFT        2
#define PRIO_FILTER     2
#define PRIO_COMM       1
#define PRIO_MONITOR    1

// ======= MONITOR =======
#define INA219_POLL_MS  100     // INA219 read interval
