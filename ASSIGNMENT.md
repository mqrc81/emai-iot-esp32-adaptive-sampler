The goal of the assignment is to create an IoT system that collects information from a sensor, analyses the data locally
and communicates to a nearby server an aggregated value of the sensor readings. The IoT system adapts the sampling
frequency in order to save energy and reduce communication overhead. The IoT device will be based on an ESP32 prototype
board and the firmware will be developed using the FreeRTOS. You are free to use IoT-Lab or real devices.

### Input

Assume an input signal of the form of `SUM(a_k*sin(f_k))`.
For example: `2*sin(2*pi*3*t)+4*sin(2*pi*5*t)`

### Maximum sampling frequency

Identify the maximum sampling frequency of your hardware device, for example, 100Hz. Note 100Hz is only an example. You
need to demonstrate the ability of sampling at a very high frequency.

### Identify optimal sampling frequency

Compute the FFT and adapt the sampling frequency accordingly. For example, for a maximum frequency of 5 Hz adapt the
sampling frequency to 10Hz.

### Compute aggregate function over a window

Compute the average of the sampled signal over a window, for example, 5 secs.

### Communicate the aggregate value to the nearby server

Transmit the aggregate value, i.e. the average, to a nearby edge server using MQTT over WIFI.

### Communicate the aggregate value to the cloud

Transmit the aggregate value, i.e. the average, to a cloud server using LoRaWAN + TTN.

### Measure the performance of the system

* Evaluate the savings in energy of the new/adaptive sampling frequency against the original over-sampled one. Note that
  in some cases, the optimized sampling frequency cannot be employed due to the latencies of sleeping policies.
* Measure per-window execution time.
* Measure the volume of data transmitted over the network when the new/adaptive sampling frequency is used against the
  original over sampled one.
* Measure the end-to-end latency of the system. From the point the data are generated up to the point they are received
  from the edge server.

### Bonus

* Consider at least 3 different input signals and measure the performance of the system. Discuss different types of an
  input signal may affect the overall performance in the case of adaptive sampling vs basic/over-sampling.
* In addition to the original clean sinusoidal signal, use an alternative noisy signal type that models : s(t) = 2*sin(
  2*pi*3*t)+4*sin(2*pi*5*t) + n(t) + A(t) where n(t) is Gaussian noise with small sigma (e.g., σ=0.2) modelling sensor
  baseline noise and A(t) is an anomaly injection component: a sparse random spike process where, with low probability
  p (e.g. p = 0.02 per sample), a large-magnitude outlier +/- U(5, 15) is injected that models transient hardware
  faults, EMI interference, or physical disturbances. Introduce an anomaly-aware filter over a given window using (a)
  Z-score (assumes low contamination of spikes and mainly Gaussian noise) and (b) Hamper (preferable when anomaly
  injection rate is high). Evaluate the detection performance of each of the two filters using (since anomalies are
  synthetically injected, their positions are known): True Positive Rate (TPR) - Fraction of injected anomalies
  correctly flagged; False Positive Rate (FPR) - Fraction of clean samples incorrectly flagged; Mean Error Reduction
  across different anomaly injection rates (p=1%, 5%, 10%). Measure the execution time and the energy impact of of each
  filter. Discuss the impact of anomaly spikes on the in the FFT. Measure and compare the FFT-estimated dominant
  frequency anomaly-contaminated signal (unfiltered) and the FFT-estimated dominant frequency anomaly pre-filtering.
  Discuss the resulting adaptive sampling frequency difference and its energy impact with and without filtering. Measure
  the effect of the window size of the filter on the performance of system in terms of computational effort, end-to-end
  delay increase, memory usage. Larger windows improve statistical estimates but increase latency and memory use.
  Characterize this trade-off empirically.
