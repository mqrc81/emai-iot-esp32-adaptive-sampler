#include "fft.hpp"
#include <Arduino.h>
#include <arduinoFFT.h>

static float fftReal[2048];
static float fftImag[2048];

FFTResult computeFFT(const float *samples, int size, float sampleRateHz) {
    if (size > 2048) {
        Serial.println("{\"phase\":\"FFT\",\"error\":\"size too big\"}");
        return {0.0f, 0.0f};
    }

    for (int i = 0; i < size; i++) {
        fftReal[i] = samples[i];
        fftImag[i] = 0.0f;
    }

    ArduinoFFT<float> fft(fftReal, fftImag, size, sampleRateHz);
    fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    fft.compute(FFTDirection::Forward);
    fft.complexToMagnitude();

    float maxMag = 0.0f;
    int dominantBin = 1;
    for (int i = 1; i < size / 2; i++) {
        if (fftReal[i] > maxMag) {
            maxMag = fftReal[i];
            dominantBin = i;
        }
    }

    float freqResolution = sampleRateHz / size;
    float dominantFreq = dominantBin * freqResolution;

    FFTResult result;
    result.dominantFrequency = dominantFreq;
    result.optimalSampleRate = 2.0f * dominantFreq;
    return result;
}
