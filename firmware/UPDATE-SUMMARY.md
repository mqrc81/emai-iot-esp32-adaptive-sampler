## Hardware Setup

```
USB Power
    ↓
INA219 (I2C: SDA=GPIO17, SCL=GPIO18)
    ↓ measures current/voltage/power
Heltec WiFi LoRa 32 V3
├── ESP32-S3 @ 240MHz
├── SX1262 (internal, SPI: NSS=8, DIO1=14, RST=12, BUSY=13)
├── WiFi (internal)
├── OLED display (optional status)
└── ADC pins (not used for signal — mathematical generation)
```

INA219 wiring:

```
Heltec Ve pin  → INA219 VIN+
INA219 VIN-    → breadboard rail
INA219 VCC     → Heltec 3V3
INA219 GND     → Heltec GND
INA219 SDA     → GPIO17
INA219 SCL     → GPIO18
```

---

## FreeRTOS Task Architecture

```
┌─────────────────────────────────────────────────────┐
│                    QUEUES                           │
│  sampleQueue  → FFTTask                             │
│  rateQueue    → SamplerTask (adaptive rate update)  │
│  resultQueue  → CommTask                            │
│  powerQueue   → CommTask (energy readings)          │
└─────────────────────────────────────────────────────┘

┌─────────────────┐
│  SamplerTask    │  Priority: 3 (highest)
│  Stack: 4KB     │
│                 │  - Generates signal mathematically
│                 │  - Samples at current rate
│                 │  - Fills FFT buffer (2048 samples)
│                 │  - Sends buffer to FFTTask via queue
│                 │  - Waits for adaptive rate from FFTTask
│                 │  - Fills window buffer (5s worth)
│                 │  - Sends window to FilterTask
└────────┬────────┘
         │ xQueueSend(sampleQueue, buffer)
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
│                 │  - Signals FilterTask to start window
└────────┬────────┘
         │ xQueueSend(rateQueue, adaptiveRate)
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
         │ xQueueSend(resultQueue, result)
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
│                 │  - Sends readings to CommTask
└─────────────────┘
```

---

## Inter-Task Communication

```cpp
// Queue definitions
QueueHandle_t sampleQueue;  // SamplerTask → FFTTask: float buffer
QueueHandle_t rateQueue;    // FFTTask → SamplerTask: float (adaptive rate)
QueueHandle_t windowQueue;  // SamplerTask → FilterTask: float buffer
QueueHandle_t resultQueue;  // FilterTask → CommTask: WindowResult struct
QueueHandle_t powerQueue;   // MonitorTask → CommTask: PowerReading struct

// Structs passed through queues
struct WindowResult {
    float    average;
    float    adaptiveRate;
    int      sampleCount;
    float    computeMs;
    int64_t  timestampUs;   // NTP Unix time in microseconds
    int      signalIndex;   // which of 3 signals
    int      filterType;    // 0=none, 1=zscore, 2=hampel
    float    anomalyProb;   // injection rate
    float    tpr;
    float    fpr;
    float    meanError;
};

struct PowerReading {
    float currentMa;
    float voltageV;
    float powerMw;
    float energyMj;         // accumulated since phase start
    char  phase[32];        // "OVERSAMPLE", "ADAPTIVE", "IDLE" etc
};
```

---

## Experiment Phases

**Phase 0 — Initialisation**

- `Serial.begin(115200)` — debug output
- INA219 init via I2C
- WiFi connect → NTP sync → MQTT connect
- SX1262 init via RadioLib
- LoRaWAN OTAA join with TTN
- Start all FreeRTOS tasks
- MonitorTask begins continuous INA219 logging

**Phase 1 — Max Rate Benchmark**

- SamplerTask runs tight loop, no delays
- MonitorTask tags phase as "BENCHMARK"
- Records achieved Hz via `esp_timer_get_time()`
- Records real power draw at max rate
- Duration: 10000 samples

**Phase 2 — FFT & Adaptive Rate**

- SamplerTask collects 2048 samples at 1000Hz
- FFTTask processes → dominant frequency → adaptive rate
- Result published via MQTT and LoRaWAN
- MonitorTask logs power during FFT computation

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
  "energy_mj": 12.4
}
```

- CommTask sends Cayenne LPP uplink via LoRaWAN
- E2E latency = Mac receipt time - `timestamp_us`

**Phase 4 — Noisy Signal, Multiple Injection Rates**

- Repeat for p = 1%, 5%, 10%
- For each: Z-score filter then Hampel filter
- TPR/FPR computed against known ground truth
- Results published per window

**Phase 5 — Window Size Trade-off**

- W = 5, 11, 21, 51, 101, 201
- At p = 5%
- Execution time measured via `esp_timer_get_time()`
- Memory usage logged (W × 4 bytes)

**Phase 6 — Three Signals Comparison**

- Signal 1: `2sin(2π·3t) + 4sin(2π·5t)` (baseline)
- Signal 2: `4sin(2π·2t)` (low frequency)
- Signal 3: `2sin(2π·10t) + 3sin(2π·45t)` (high frequency)
- Full FFT + adaptive rate + windowed average for each
- Energy measured per signal

**Phase 7 — LoRaWAN Duty Cycle Validation**

- Count actual uplinks sent
- Log airtime per packet
- Compare against TTN fair use limit

---

## Changes from Current Firmware

| Component        | Current (`main.cpp`)                        | New                                   |
|------------------|---------------------------------------------|---------------------------------------|
| Architecture     | Sequential in `setup()`                     | FreeRTOS tasks with queues            |
| Board            | `arduino_nano_esp32` / `esp32-s3-devkitc-1` | `heltec_wifi_lora_32_V3`              |
| Signal source    | Mathematical                                | Mathematical (unchanged)              |
| FFT library      | arduinoFFT                                  | arduinoFFT (unchanged)                |
| FFT buffers      | Stack allocated                             | Heap allocated via `heap_caps_malloc` |
| Task stack sizes | N/A                                         | Explicitly set per task               |
| Energy           | Calculated ratio                            | Real INA219 readings                  |
| MQTT             | Not implemented                             | PubSubClient over WiFi                |
| LoRaWAN          | Stubbed                                     | Real SX1262 → TTN via RadioLib        |
| Timestamps       | Boot-relative                               | NTP Unix time                         |
| Max sample rate  | Derived                                     | Measured in benchmark loop            |
| OLED             | Not used                                    | Optional live status display          |
| Pin mapping      | Nano ESP32                                  | Heltec V3 known pins                  |

---

## Key Constants in `config.h`

```cpp
// Heltec WiFi LoRa 32 V3 SX1262 pins
#define LORA_NSS    8
#define LORA_DIO1  14
#define LORA_RESET 12
#define LORA_BUSY  13
#define LORA_SCK    9
#define LORA_MOSI  10
#define LORA_MISO  11

// INA219 I2C pins
#define INA219_SDA 17
#define INA219_SCL 18

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

## Open Questions Before Writing Code

1. **INA219 wiring** — do you have the breakout board with VIN+/VIN- terminals, or just the chip?
2. **TTN keys** — DevEUI/AppEUI/AppKey from TTN console
3. **Home WiFi SSID/password** for MQTT phase
4. **Library WiFi SSID/password** for LoRaWAN phase
5. **Heltec V3 pin confirmation** — the pin mapping above is from the datasheet you shared earlier, but worth
   double-checking against the Heltec library