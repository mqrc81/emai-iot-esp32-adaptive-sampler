#include "fft.hpp"
#include "logger.hpp"
#include <arduinoFFT.h>
#include <cstdlib>

FFTResult computeFFT(const float *samples, int size, float sampleRateHz) {
    float *real = (float *) malloc(size * sizeof(float));
    float *imag = (float *) malloc(size * sizeof(float));

    if (!real || !imag) {
        free(real);
        free(imag);
        logMsg("[FFT] malloc failed");
        return {0.0f, 0.0f};
    }

    for (int i = 0; i < size; i++) {
        real[i] = samples[i];
        imag[i] = 0.0f;
    }

    ArduinoFFT<float> fft(real, imag, size, sampleRateHz);
    fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    fft.compute(FFTDirection::Forward);
    fft.complexToMagnitude();

    float maxMag = 0.0f;
    int dominantBin = 1;
    for (int i = 1; i < size / 2; i++) {
        if (real[i] > maxMag) {
            maxMag = real[i];
            dominantBin = i;
        }
    }

    float freqResolution = sampleRateHz / size;
    float dominantFreq = dominantBin * freqResolution;

    free(real);
    free(imag);

    logFmt("[FFT] dominant=%.2fHz adaptive=%.2fHz", dominantFreq, 2.0f * dominantFreq);
    return {dominantFreq, 2.0f * dominantFreq};
}
