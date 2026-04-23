#include <stdio.h>
#include "signal.h"
#include "fft.h"
#include "sampler.h"
#include "lorawan_client.h"

#define MAX_SAMPLE_RATE  1000.0f
#define FFT_SIZE         2048
#define WINDOW_SECS      5.0f

int main()
{
    // FFT to find adaptive rate
    float fftBuf[FFT_SIZE];
    for (int i = 0; i < FFT_SIZE; i++)
        fftBuf[i] = generateSignal(i / MAX_SAMPLE_RATE);
    FFTResult fft = computeFFT(fftBuf, FFT_SIZE, MAX_SAMPLE_RATE);

    // Compute window
    WindowResult window = computeWindow(fft.optimalSampleRate, WINDOW_SECS);

    // LoRaWAN config — these would be provisioned in TTN console
    LoRaWANConfig config;
    config.devEUI = "0102030405060708";
    config.appEUI = "0807060504030201";
    config.appKey = "01020304050607080102030405060708";
    config.dataRate = 5; // DR5 = SF7, fastest/shortest range
    config.port = 1;

    lorawanPublishWindow(config, window, WINDOW_SECS);

    return 0;
}
