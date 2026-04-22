#include <stdio.h>
#include "signal.h"
#include "fft.h"
#include "sampler.h"
#include "mqtt_client.h"

#define MAX_SAMPLE_RATE  100.0f
#define WINDOW_SECS      5.0f
#define FFT_SIZE         256

int main()
{
    // FFT to find optimal rate
    float fftBuffer[FFT_SIZE];
    for (int i = 0; i < FFT_SIZE; i++)
        fftBuffer[i] = generateSignal(i / MAX_SAMPLE_RATE);
    FFTResult fft = computeFFT(fftBuffer, FFT_SIZE, MAX_SAMPLE_RATE);

    printf("Adaptive rate: %.2f Hz\n", fft.optimalSampleRate);

    // Compute window at adaptive rate
    WindowResult window = computeWindow(fft.optimalSampleRate, WINDOW_SECS);

    // Publish via MQTT
    MQTTConfig config;
    config.host = "localhost";
    config.port = 1883;
    config.topic = "iot/window";
    config.clientId = "iot-sampler-001";

    mqttPublishWindow(config, window, fft.optimalSampleRate);

    return 0;
}
