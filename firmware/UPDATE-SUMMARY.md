## Updated Summary

### Hardware Setup

```
Wall socket → USB-A charger → cut USB-A to USB-C cable
                                      ↓
                        Red wire → INA219 VIN+
                        INA219 VIN- → Red wire → Heltec USB-C port
                        Black wires joined directly
                        Data wires (white/green) joined directly
                                      ↓
                              Heltec WiFi LoRa 32 V3.2
                              ├── ESP32-S3 @ 240MHz
                              ├── SX1262 (internal)
                              ├── WiFi (internal)
                              └── OLED display
```

INA219 I2C wiring:

```
INA219 VCC → Heltec 3V3
INA219 GND → Heltec GND
INA219 SDA → Heltec SDA
INA219 SCL → Heltec SCL
INA219 VIN+ → Red wire USB-A side (cut cable)
INA219 VIN- → Red wire USB-C side (cut cable)
330Ω resistor → REMOVED
```

**What INA219 measures:** total board current including ESP32-S3 core, WiFi radio, SX1262 LoRa radio — everything
powered by the USB input.

**Workflow:**

1. Flash firmware via laptop → USB-C to USB-C cable
2. Disconnect laptop cable
3. Connect cut cable through INA219 → wall socket for experiment
4. All results published via MQTT over WiFi (no Serial during experiment)
5. Reconnect laptop cable if Serial output needed

---

### FreeRTOS Task Architecture

```
┌─────────────────┐
│  SamplerTask    │  Priority: 3 (highest)
│  Stack: 12KB    │
│                 │  - Generates signal mathematically
│                 │  - Samples at current rate
│                 │  - Fills FFT buffer (2048 samples)
│                 │  - Sends buffer to FFTTask via queue
│                 │  - Receives adaptive rate from FFTTask
│                 │  - Fills window buffer (5s worth)
│                 │  - Sends window to FilterTask
└────────┬────────┘
         │ xQueueSend(sampleQueue)
         ▼
┌─────────────────┐
│  FFTTask        │  Priority: 2
│  Stack: 24KB    │
│                 │  - Receives 2048-sample buffer
│                 │  - Heap-allocates FFT buffers
│                 │  - Runs arduinoFFT
│                 │  - Identifies dominant frequency
│                 │  - Computes adaptive rate (Nyquist)
│                 │  - Sends rate back to SamplerTask
└────────┬────────┘
         │ xQueueSend(rateQueue)
         ▼
┌─────────────────┐
│  FilterTask     │  Priority: 2
│  Stack: 8KB     │
│                 │  - Receives window samples
│                 │  - Applies Z-score or Hampel filter
│                 │  - Computes windowed average
│                 │  - Records NTP timestamp
│                 │  - Sends result to CommTask
└────────┬────────┘
         │ xQueueSend(resultQueue)
         ▼
┌─────────────────┐
│  CommTask       │  Priority: 1
│  Stack: 8KB     │
│                 │  - Receives windowed result
│                 │  - Publishes JSON via MQTT over WiFi
│                 │  - Encodes Cayenne LPP
│                 │  - Transmits via SX1262 → TTN
│                 │  - Records MQTT publish timestamp
└─────────────────┘

┌─────────────────┐
│  MonitorTask    │  Priority: 1
│  Stack: 4KB     │
│                 │  - Reads INA219 every 100ms
│                 │  - Logs current/voltage/power
│                 │  - Accumulates energy per phase
│                 │  - Publishes via MQTT
└─────────────────┘
```

---

### Inter-Task Communication

```cpp
QueueHandle_t sampleQueue;  // SamplerTask → FFTTask
QueueHandle_t rateQueue;    // FFTTask → SamplerTask
QueueHandle_t windowQueue;  // SamplerTask → FilterTask
QueueHandle_t resultQueue;  // FilterTask → CommTask
QueueHandle_t powerQueue;   // MonitorTask → CommTask

struct WindowResult {
    float   average;
    float   adaptiveRate;
    int     sampleCount;
    float   computeMs;
    int64_t timestampUs;    // NTP Unix time microseconds
    int     signalIndex;    // which of 3 signals
    int     filterType;     // 0=none, 1=zscore, 2=hampel
    float   anomalyProb;    // injection rate
    float   tpr;
    float   fpr;
    float   meanError;
};

struct PowerReading {
    float currentMa;
    float voltageV;
    float powerMw;
    float energyMj;         // accumulated since phase start
    char  phase[32];        // "BENCHMARK", "ADAPTIVE", "FFT" etc
};
```

---

### Experiment Phases

**Phase 0 — Initialisation**

- INA219 init via I2C
- WiFi connect → NTP sync → MQTT connect
- SX1262 init via RadioLib
- LoRaWAN OTAA join with TTN
- Start all FreeRTOS tasks
- MonitorTask begins continuous INA219 logging

**Phase 1 — Max Rate Benchmark**

- SamplerTask tight loop, no delays
- MonitorTask tags phase "BENCHMARK"
- Records achieved Hz via `esp_timer_get_time()`
- Records real power draw at max rate via INA219

**Phase 2 — FFT & Adaptive Rate**

- SamplerTask collects 2048 samples at 1000Hz
- FFTTask processes → dominant frequency → adaptive rate
- MonitorTask logs power during FFT computation
- Result published via MQTT

**Phase 3 — Windowed Average (clean signal)**

- SamplerTask collects at adaptive rate over 5s
- FilterTask computes average (no filter)
- CommTask publishes JSON via MQTT:

```json
{
  "phase": "WINDOW_CLEAN",
  "window": 1,
  "average": 0.051832,
  "sample_rate": 9.77,
  "sample_count": 48,
  "compute_ms": 0.004,
  "timestamp_us": 1714915200000000,
  "current_ma": 45.2,
  "power_mw": 149.2,
  "energy_mj": 12.4
}
```

- CommTask sends Cayenne LPP uplink via LoRaWAN
- E2E latency = Mac receipt time - `timestamp_us`

**Phase 4 — Noisy Signal, Multiple Injection Rates**

- Repeat for p = 1%, 5%, 10%
- Z-score filter then Hampel filter
- TPR/FPR computed against known ground truth

**Phase 5 — Window Size Trade-off**

- W = 5, 11, 21, 51, 101, 201 at p = 5%
- Execution time via `esp_timer_get_time()`
- Memory usage logged (W × 4 bytes)

**Phase 6 — Three Signals Comparison**

- Signal 1: `2sin(2π·3t) + 4sin(2π·5t)` (baseline)
- Signal 2: `4sin(2π·2t)` (low frequency)
- Signal 3: `2sin(2π·10t) + 3sin(2π·45t)` (high frequency)
- Full FFT + adaptive rate + windowed average + INA219 energy per signal

**Phase 7 — LoRaWAN at library (TTN coverage)**

- Travel to location with TTN gateway coverage
- Same firmware, different WiFi credentials in config.h
- Real OTAA join + uplink
- Confirm receipt in TTN console

---

### Changes from Current Firmware

| Component         | Current                 | New                               |
|-------------------|-------------------------|-----------------------------------|
| Board             | `esp32-s3-devkitc-1`    | `heltec_wifi_lora_32_V3`          |
| Architecture      | Sequential in `setup()` | FreeRTOS tasks with queues        |
| Signal source     | Mathematical            | Mathematical (unchanged)          |
| FFT buffers       | Stack allocated         | Heap via `heap_caps_malloc`       |
| Task stack sizes  | N/A                     | Explicitly set per task           |
| Energy            | Calculated ratio        | Real INA219 — total board current |
| MQTT              | Not implemented         | PubSubClient over WiFi            |
| LoRaWAN           | Stubbed                 | Real SX1262 → TTN via RadioLib    |
| Timestamps        | Boot-relative           | NTP Unix time                     |
| Max sample rate   | Derived                 | Measured in benchmark loop        |
| OLED              | Not used                | Optional live status              |
| RadioLib conflict | pin remap error         | Resolved via correct board target |

---

### config.h Constants

```cpp
// Heltec WiFi LoRa 32 V3.2 SX1262 pins
#define LORA_NSS    8
#define LORA_DIO1  14
#define LORA_RESET 12
#define LORA_BUSY  13
#define LORA_SCK    9
#define LORA_MOSI  10
#define LORA_MISO  11

// INA219 I2C pins
#define INA219_SDA  17
#define INA219_SCL  18

// WiFi (update per location)
#define WIFI_SSID     "your_ssid" // TODO
#define WIFI_PASSWORD "your_password" // TODO

// MQTT
#define MQTT_HOST  "mac_local_ip"
#define MQTT_PORT  1883
#define MQTT_TOPIC "iot/window"

// TTN
#define TTN_DEV_EUI  "0000000000000000"
#define TTN_APP_EUI  "0000000000000000"
#define TTN_APP_KEY  "00000000000000000000000000000000"

// Experiment
#define MAX_SAMPLE_RATE    1000.0f
#define FFT_SIZE           2048
#define WINDOW_SECS        5.0f
#define NUM_WINDOWS        5
#define FILTER_WINDOW      21
#define ZSCORE_THRESH      3.0f
#define HAMPEL_THRESH      3.0f
#define BENCHMARK_SAMPLES  10000
```

---
