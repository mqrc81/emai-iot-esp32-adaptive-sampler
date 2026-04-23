# ESP32 Adaptive Sampler

## Goal

The goal is to create an IoT system that collects information from a sensor, analyses the data locally and communicates
to a nearby server an aggregated value of the sensor readings. The IoT system adapts the sampling frequency in order to
save energy and reduce communication overhead. The IoT device will be based on an ESP32 prototype board and the firmware
will be developed using the FreeRTOS.

---

## Table of Contents

1. [System Overview](#system-overview)
2. [Methodology & Limitations](#methodology--limitations)
3. [Setup & Build](#setup--build)
4. [Maximum Sampling Frequency](#maximum-sampling-frequency)
5. [FFT & Adaptive Sampling](#fft--adaptive-sampling)
6. [Windowed Average](#windowed-average)
7. [MQTT](#mqtt)
8. [LoRaWAN / TTN](#lorawan--ttn)
9. [Performance Evaluation](#performance-evaluation)
10. [3 Input Signals (Bonus)](#3-input-signals-bonus)
11. [Noisy Signal & Anomaly Detection (Bonus)](#noisy-signal--anomaly-detection-bonus)
12. [FFT Contamination (Bonus)](#fft-contamination-bonus)
13. [Window Size Trade-off (Bonus)](#window-size-trade-off-bonus)
14. [LLM Documentation](#llm-documentation)

---

## System Overview

The system represents adaptive IoT sampling with the following stages:

```
Signal Generation (1000 Hz)
        ↓
FFT Analysis (2048 samples)
→ Identify dominant frequency
→ Compute optimal sampling rate (Nyquist)
        ↓
Adaptive Sampling (~10 Hz over 5s window)
→ Optional anomaly filtering (Z-score / Hampel)
→ Compute windowed average
        ↓
MQTT Publish → Local Mosquitto Broker (edge)
LoRaWAN Stub → TTN (cloud)
```

**Hardware target**: ESP32 DevKit V1

**Framework**: FreeRTOS (simulated natively on macOS for development and evaluation)

---

## Methodology & Limitations

### Implementation Approach

This project was implemented in native C++ on macOS rather than
on physical ESP32 hardware. Signal processing, FFT, and anomaly detection results
are mathematically equivalent to executing it on an ESP32 device and independent of hardware.

### IoT-Lab Attempt

I attempted real hardware validation using FIT IoT-Lab with the Arduino Nano
ESP32 node at Grenoble. Despite multiple flashing attempts (firmware.bin,
firmware.elf, merged binary) and submission methods, the node consistently
reported a deployment error. The IoT-Lab API confirmed the node state as "Alive",
indicating a hardware fault on the testbed infrastructure. Serial port forwarding
and the MQTT serial bridge both returned ConnectionRefusedError(111). As only one
Arduino Nano ESP32 node exists, I was unable to complete real hardware benchmarking.

### Simulation Limitations

| Metric            | Status                  | Caveat                                        |
|-------------------|-------------------------|-----------------------------------------------|
| Max sampling rate | Derived                 | Based on ESP32 datasheet + FreeRTOS tick rate |
| Energy saving     | Calculated              | Upper bound on sampling energy only           |
| E2E latency       | Measured (Mac loopback) | Real WiFi latency would be 1–50ms             |
| MQTT              | Real broker (Mosquitto) | Protocol is real; WiFi stack not exercised    |
| LoRaWAN           | Stubbed                 | Hardware unavailable; encoding is correct     |
| Signal processing | Exact                   | Hardware-independent mathematical results     |

---

## Setup & Build

### Simulation

#### Prerequisites

```bash
brew install mosquitto cmake
```

#### Build

```bash
mkdir build && cd build
cmake .. && make
```

#### Run all tests

```bash
# Terminal 1: MQTT broker
mosquitto -v

# Terminal 2: subscribe to MQTT
mosquitto_sub -t "iot/window" -v

# Terminal 3: run tests
cd build
./test_signal > ../scripts/signal.csv
./test_signal_noisy > ../scripts/noisy_signal.csv
./test_fft
./test_sampler
./test_mqtt
./test_filter
./test_fft_contamination
./test_window_tradeoff
./test_signals_comparison
./test_lorawan
```

Note that the output of all tests is available in [/results](/results).

#### Plot signals

```bash
cd scripts
python3 plot_signal.py
python3 plot_signal_noisy.py
```

### Arduino Firmware

#### IoT-Lab (Attempted)

The firmware can be built for the Arduino Nano ESP32 node on FIT IoT-Lab:

1. Sign up to IoT-Lab (with login & password)
2. Add your SSH key in profile section
3. Create a new experiment in the testbed with a node using Arduino-Nano-ESP32 architecture

Replace `<login>`, `<password>`, and `<node_id>` with values from IoT-Lab.

```bash
# Terminal 1
mosquitto_sub --insecure -h mqtt4.iot-lab.info -p 8883 \                                                                 ✘ INT  30s 
  -u mschmidt -P <password> \
  -t "iotlab/<mschmidt>/#" -v | tee ./results/iotlab/results.json

# Terminal 2
cd firmware && pio run && scp ./.pio/build/arduino_nano_esp32/firmware.bin <login>@grenoble.iot-lab.info:~/firmware.bin

# Terminal 3
ssh <login>@grenoble.iot-lab.info

iotlab-node --flash ~/firmware.bin -l grenoble,arduino-nano-esp32,<node_id> -u <login>

iotlab_mqtt_bridge
```

> **Note:** The Arduino Nano ESP32 node at Grenoble (arduino-nano-esp32-1)
> consistently reported deployment error during testing, indicating a hardware
> fault on the testbed. See [Methodology & Limitations](#methodology--limitations)
> for details.

#### ESP32 Device (Not Attempted)

```bash
cd firmware 
pio run
pio run --target upload
pio device monitor    
```

---

## Maximum Sampling Frequency

Since this system is simulated, instead of identifying the max sampling frequency by benchmarking the ESP32, I derived
it from the ESP32 datasheet and FreeRTOS numbers.

According to Espressif Technical Reference Manual, Section 29: The ESP32 ADC supports up to 2MHz, but in FreeRTOS
firmware, sampling is task-driven. Since the default FreeRTOS tick rate is 1000Hz, a sampling task can't wake more often
than once per tick, making 1000Hz the upper bound, which is what I used as the max sampling rate.

On a real ESP32, this could be benchmarked using `esp_timer_get_time()` in a sampling loop; but the chosen max sampling
rate is a conservative estimate which is still grounded in data.

---

## FFT & Adaptive Sampling

The FFT size was chosen to provide enough resolution to distinguish the 3Hz from the 5Hz frequency. The resolution is
calculated using: `resolution = sample_rate / FFT_size`

Initially an FFT size of 256 was chosen, which led to a resolution of 3.91Hz, which was not capable of cleanly
distinguishing 3Hz from 5Hz.
Increasing to 2048 samples leads to a clear separation since the multiples of the resolution of 0.488Hz are close enough
to the true fequency.

### Results

```
Dominant frequency : 4.88 Hz
Optimal sample rate: 9.77 Hz
Original sample rate: 1000.00 Hz
Rate reduction factor: 102.4x
```

The 0.12Hz error (4.88 vs 5.00Hz) is explainable by the aforementioned resolution of 0.488Hz. 5Hz falls between 4.883Hz
and 5.371Hz.

The FFT correctly identifies 4.88Hz as dominant (10th bin). The Nyquist rate of 9.77Hz still safely exceeds the true 5Hz
frequency, so the deviation does not mean incorrect results.

The dominant frequency of **5Hz** (amplitude 4) as opposed to 3Hz (amplitude 2) is consistent with the signal
definition.

### Adaptive Rate Logic

Per the Nyquist-Shannon sampling theorem, a signal must be sampled at at least twice its highest frequency:

```
f_optimal = 2 * 4.88Hz = 9.77Hz
```

This replaces the initial 1000Hz baseline rate and is used for all subsequent tests.

---

## Windowed Average

Samples are collected at the adaptive rate over a fixed 5-second window and averaged:

```
samples = 9.77Hz * 5s = 48
```

### Results

| Metric   | Max Rate (1000Hz) | Adaptive Rate (9.77Hz) |
|----------|-------------------|------------------------|
| Samples  | 5000              | 48                     |
| Average  | 0.000000          | 0.051832               |
| Duration | 0.341 ms          | 0.003 ms               |

This reports a sample reduction of 102.4x.

The average difference of 0.051832 is a sampling alignment error: 48 samples at 9.77Hz do not land on symmetric points
of the sinusoid, leaving a small difference. At 5000 samples this cancels out.

---

## MQTT

The windowed average is published to the local Mosquitto broker over TCP. The MQTT connection is established once before
the measurement loop and reused across all windows. Prior to this optimisation the first window's duration was
significantly larger than the rest which led to skewed results.

### Results

To get a fair average, the MQTT communication was tested over 10 windows.

```
Connected to broker at localhost:1883

Window   Avg          Samples      Compute(ms)  E2E(ms)     
1        0.051832     48           0.006        0.061       
2        0.051832     48           0.005        0.150       
3        0.051832     48           0.005        0.036       
4        0.051832     48           0.005        0.149       
5        0.051832     48           0.005        0.034       
6        0.051832     48           0.005        0.027       
7        0.051832     48           0.004        0.253       
8        0.051832     48           0.006        0.125       
9        0.051832     48           0.005        0.049       
10       0.051832     48           0.005        0.026  
     
Disconnected from broker

Avg compute time    : 0.005 ms
Avg E2E latency     : 0.091 ms
```

This reports energy and data volume savings of 99.0% over the baseline rate.

---

## LoRaWAN / TTN

LoRaWAN transmission to TTN was implemented as a stub. The Wokwi simulator does not support LoRa and a physical
transceiver was unavailable.

### Payload Encoding

I chose **Cayenne Low Power Payload** as the encoding format because TNN supports it natively and it is much more
compact
than pure JSON payload (4B vs 60B for the average value). This matters because LoRaWAN has strict payload size limits (
51B).

## Results

```
Payload : 01 02 00 05 (4 bytes, Cayenne LPP)
Decoded avg : 0.051832
```

The stub also highlights a key constraint: with a 5-second window interval, 17280 uplinks per day would result in about
345'600ms of airtime, which is far greater than TTN's fair use policy allows. In a real deployment, transmissions would
need to be less frequent.

---

## Performance Evaluation

### Summary Table

| Metric                | Value   | Notes                                 |
|-----------------------|---------|---------------------------------------|
| Max sample rate       | 1000Hz  | Derived from ESP32 datasheet          |
| Adaptive sample rate  | 9.77Hz  | From FFT dominant frequency           |
| Rate reduction factor | 102.4x  |                                       |
| Energy saving         | 99.0%   | Proportional to sample rate reduction |
| Data volume saving    | 99.0%   | Raw buffer: 20000 to 192 B/window     |
| Average duration      | 0.005ms | Per 5s window                         |
| Average E2E latency   | 0.091ms | Mac loopback                          |

### Energy Saving Methodology

Energy saving is calculated as the reduction in active sampling time:

```
energy_saving = 1 - (9.77Hz / 1000Hz) = 99.0%
```

This is the upper bound on energy used to sample; other energy expenditure like WiFi and idle time are not reported by
this simulation but would most likely reduce the actual energy saving on an ESP32 device.

### Data Volume

```
Baseline: 5000 samples * 4B = 20'000 B/window (raw buffer)
Adaptive: 48 samples * 4B = 192 B/window (raw buffer)
MQTT payload: 60 B/window (JSON only)
```

For MQTT, only the aggregated JSON is transmitted, not the raw buffer. Otherwise the actual saving would be even greater
than 99%.

### End-to-End Latency

E2E latency is measured from the start of signal generation until the MQTT publish is acknowledged. The average of
0.091ms represents the loopback of my Mac to the local Mostquitto broker. On a real ESP32 device, WiFi transmission
would likely be even greater (multiple ms depending on network conditions and location of MQTT broker server).

---

## 3 Input Signals (Bonus)

3 signals with different frequency characteristics were chosen to evaluate how they affect the adaptive frequency.

### Results

| Signal                        | Dominant Freq | Adaptive Rate | Reduction | Energy Saving |
|-------------------------------|---------------|---------------|-----------|---------------|
| `2sin(2π·3t) + 4sin(2π·5t)`   | 4.88Hz        | 9.77Hz        | 102.4x    | 99.0%         |
| `4sin(2π·2t)`                 | 1.95Hz        | 3.91Hz        | 256.0x    | 99.6%         |
| `2sin(2π·10t) + 3sin(2π·45t)` | 44.92Hz       | 89.84Hz       | 11.1x     | 91.0%         |

### Discussion

**Signal 1:** The baseline signal: 2 low-frequency components with different amplitudes. FFT correctly identifies the
5Hz as dominant (higher amplitude). 102.4x reduction shows how important adaptive sampling is.

**Signal 2:** The best-case signal: 1 low-frequency 2Hz sinus requires only around 4Hz sampling, resulting in reduction
of 256x and 99.6% energy saving.

**Signal 3:** The worst-case signal: 1 high-frequency 45Hz sinus dominates the other low-frequency 10Hz sinus, which
leads to an adaptive rate of 90Hz, which only reduces the max sampling rate by 11.1x. Even in this worst case, adaptive
sampling still saves 91% compared to oversampling at 1000Hz.

This shows that how effective adaptive sampling is, is decided entirely by the dominant frequency of the signal. Signals
with low dominant frequencies benefit the most, but even signals with high dominant frequencies have huge savings.

---

## Noisy Signal & Anomaly Detection (Bonus)

### Signal Model

```
s(t) = 2sin(2π·3t) + 4sin(2π·5t) + n(t) + A(t)
```

- `n(t)` = Gaussian noise with σ=0.2 (models baseline noise of sensor)
- `A(t)` = Sparse spike process with probability p per sample, which injects U(5,15) (models hardware errors and other
  unexpected physical disturbances)

At p=0.02 with seed 42: 16 anomalies were injected in 1000 samples (1.6%, which is lower than the expected 2.0% but
within the range of normal statistical deviations).

### Filter Design

**Z-score filter:** Flags sample x as anomaly if:

```
|x - mean(W)| / std(W) > k
```

**Hampel filter:** Flags sample x as anomaly if:

```
|x - median(W)| > k * 1.4826 * MAD(W)
```

The constant 1.4826 makes MAD a consistent estimator of σ for Gaussian distributions (Davies & Gather, 1993). Uses
median instead of mean — resistant to outliers even when multiple spikes are present in the window.

Both filters use a sliding window of W=21 samples (odd, so the center sample is well-defined). At 9.77 Hz adaptive rate
over a 5s window, this covers 21 consecutive samples.

The 1.4826 constant scales MAD to be comparable to the standard deviation for Gaussian noise. An advantage over Z-score
is using the median instead of the mean since the median doesn't get pulled closer towards outliers, so the filter stays
more reliable even after multiple spikes in the same window.
Both filters use a sliding window of W=21 samples. W must be odd so there's a well-defined center sample. Initially W=41
samples was chosen, but after some tuning W=21 led to almost optimal results.

### Detection Performance

#### At p=2% (baseline)

| Filter  | TPR   | FPR   | Mean Error |
|---------|-------|-------|------------|
| Z-score | 1.000 | 0.000 | 0.166      |
| Hampel  | 1.000 | 0.002 | 0.163      |

Both filters achieve near perfect detection at low injection rates. The spikes were large enough to be easily
distinguishable from signal + Gaussian noise.

#### Multiple Injection Rates (W=21, k=3.0)

**Z-score:**

| p   | TP | FP | FN | TN  | TPR   | FPR   | Mean Error |
|-----|----|----|----|-----|-------|-------|------------|
| 1%  | 7  | 0  | 0  | 973 | 1.000 | 0.000 | 0.166      |
| 5%  | 34 | 0  | 20 | 926 | 0.630 | 0.000 | 0.335      |
| 10% | 29 | 0  | 80 | 871 | 0.266 | 0.000 | 0.941      |

**Hampel:**

| p   | TP  | FP | FN | TN  | TPR   | FPR   | Mean Error |
|-----|-----|----|----|-----|-------|-------|------------|
| 1%  | 7   | 2  | 0  | 971 | 1.000 | 0.002 | 0.163      |
| 5%  | 54  | 1  | 0  | 925 | 1.000 | 0.001 | 0.155      |
| 10% | 109 | 0  | 0  | 871 | 1.000 | 0.000 | 0.155      |

### Discussion

Z-score performs worse the higher the injection rate. At p=10%, around 100 of 1000 samples are anomalies. Enough spikes
fall within each window for the anomoly detection to be less reliable, making anomalies statistically normal.
The threshold k=3.0 becomes ineffective when σ itself is corrupted.
Mean error rises from 0.17 to 0.94.

Hampel is unaffected because the median doesn't get pulled closer towards outliers, keeping mean error at 0.155 across
all rates.

In conclusion, Z-score is fine when anomaly rate is known to be low, but Hampel is the safer choice otherwise.

---

## FFT Contamination (Bonus)

### Results

| p   | Unfiltered | Z-score | Hampel  | True Rate |
|-----|------------|---------|---------|-----------|
| 1%  | 4.883Hz    | 4.883Hz | 4.883Hz | 9.770Hz   |
| 5%  | 4.883Hz    | 4.883Hz | 4.883Hz | 9.770Hz   |
| 10% | 4.883Hz    | 4.883Hz | 4.883Hz | 9.770Hz   |

### Top 5 Frequency Bins (p=10%, unfiltered)

```
bin 10:  4.883 Hz, magnitude 3477.59
bin 6:   2.930 Hz, magnitude 2031.37
bin 11:  5.371 Hz, magnitude 1219.73
bin 12:  5.859 Hz, magnitude  828.82
bin 9:   4.395 Hz, magnitude  718.61
```

### Discussion

Anomaly spikes seem to have no effect on the FFT estimation frequency, no matter injection rate. This is because sinus
energy builds up coherently across all 2048 samples, while spike energy spreads across all frequency bins equally, which
results in less overall impact.
At p=10%, around 205 spikes are present, but the 5Hz bin still dominates clearly (magnitude 3477 vs 2031 for the next
highest bin).

---

## Window Size Trade-off (Bonus)

Window sizes 5, 11, 21, 51, 101, 201 were evaluated at p=5% injection rate.

### Z-score Filter

| W   | TPR   | FPR   | Mean Error | Time    | Memory |
|-----|-------|-------|------------|---------|--------|
| 5   | 0.000 | 0.000 | 0.685      | 0.065ms | 20B    |
| 11  | 0.655 | 0.000 | 0.362      | 0.106ms | 44B    |
| 21  | 0.630 | 0.000 | 0.335      | 0.172ms | 84B    |
| 51  | 0.635 | 0.000 | 0.304      | 0.372ms | 204B   |
| 101 | 0.510 | 0.000 | 0.380      | 0.652ms | 404B   |
| 201 | 0.465 | 0.000 | 0.426      | 1.178ms | 804B   |

### Hampel Filter

| W   | TPR   | FPR   | Mean Error | Time     | Memory |
|-----|-------|-------|------------|----------|--------|
| 5   | 1.000 | 0.030 | 0.151      | 0.607ms  | 20B    |
| 11  | 1.000 | 0.007 | 0.153      | 1.279ms  | 44B    |
| 21  | 1.000 | 0.001 | 0.155      | 2.798ms  | 84B    |
| 51  | 0.885 | 0.000 | 0.195      | 7.665ms  | 204B   |
| 101 | 0.755 | 0.000 | 0.266      | 16.305ms | 404B   |
| 201 | 0.349 | 0.000 | 0.494      | 31.816ms | 804B   |

### Discussion

#### Z-score

Z-score peaks at W=11 (TPR=0.655) and becomes worse the larger the window. At p=5%, a window of W=201 contains
around 10 anomalies on average. There is no window size at which Z-score achieves a high TPR, which points to a
fundamental limitation, not a tuning problem.

#### Hampel

Hampel achieves perfect TPR up to W=21, then becomes worse at W=51 and beyond, where windows start to contain enough
anomalies to influence the median itself. **W=21** is the obvious optimal window size (TPR=1.0, FPR=0.001), with the
lowest mean error (0.155), and
an acceptable compute time (2.8ms).

#### Compute time

Z-score runs in O(W) time per sample while Hampel is O(W log W) due to the sorting required for computation of the
median. In practice, at W=201 this means 1.2ms vs 31.8ms which is a significant difference.

#### Memory

Memory grows linearly at W * 4B. At W=21, this is 84B.

#### Summary

W=21 for Hampel is validated as the optimal choice and it was selected prior to this analysis and the trade-off results
confirm it.

---

## LLM Documentation

The following summary was generated by the LLM used.

### Tool Used

Claude (Anthropic, claude-sonnet-4-6) was used as the primary implementation assistant throughout this project.

### Prompting Process

The implementation was developed through an iterative series of prompts, each building on the previous output. The
general pattern was:

1. Request explanations of a component
2. Share uncertainties and request further clarification
3. Request header file implementation of a component
4. Request partial component implementation or review (e.g. "implement the computeFFT using KissFFT" or "review and
   improve the following zscoreFilter implementation")
5. Build and run the output
6. Share errors or results for interpretation
7. Share assumptions and request corrections or code clarifications

Key prompt stages:

- CLion and Wokwi Simulator setup
- Suggested project directory structure
- Signal generation and CSV export for visual validation
- KissFFT integration and CMakeLists configuration
- Windowed average with timing using `clock_gettime`
- MQTT client with persistent connection and metrics logging
- Noisy signal generation using Box-Muller transform
- Z-score and Hampel filter implementations
- FFT contamination analysis
- Window size trade-off evaluation
- Three signal comparison with generalised `SignalDef` struct
- LoRaWAN stub with Cayenne LPP encoding

### Code Quality Assessment

**Strengths:**

- Produced correct, compilable C++ on the first attempt for most components
- Proactively identified issues such as the connection overhead affecting E2E latency measurements, and suggested the
  refactor to persistent MQTT connection
- Explanations of concepts (Nyquist, MAD constant 1.4826, Cayenne LPP) were accurate and citable
- Good at identifying when numbers needed justification (e.g. MAX_SAMPLE_RATE, FFT_SIZE, threshold k)

**Limitations:**

- Initial Wokwi serial monitor guidance was incorrect — Serial output routing in the CLion plugin was never resolved
- CMakeLists KissFFT paths were wrong due to repository structure changes since training data
- LoRaWAN airtime calculation contained a bug (used `window_ms` instead of window interval seconds)
- Cannot benchmark real hardware — all performance figures derived analytically
- Occasionally required explicit prompting to justify parameter choices rather than flagging them proactively

### Opportunities and Limitations of LLMs for IoT Development

**Opportunities:**

- Rapid scaffolding of boilerplate (CMakeLists, header/source pairs, test harnesses)
- Strong at signal processing mathematics — FFT, statistical filters, encoding formats
- Useful for interpreting results and identifying their causes (bin resolution, contamination effects)
- Effective at iterative debugging when given compiler errors or unexpected output

**Limitations:**

- Cannot interact with physical hardware or simulate real timing
- Knowledge of rapidly evolving toolchains (Wokwi CLion plugin, PlatformIO) may be outdated
- Produces plausible-looking but incorrect code for edge cases (e.g. airtime calculation bug)
- Requires domain knowledge from the user to validate correctness — blind trust would produce subtle errors
- Does not replace understanding: every result in this report was interpreted and verified by the author
