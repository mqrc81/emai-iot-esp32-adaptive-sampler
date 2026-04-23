#include "fft.hpp"
#include <Arduino.h>

FFTResult computeFFT(const float* samples, int size, float sampleRateHz)
{
    double* real = new double[size];
    double* imag = new double[size];

    for (int i = 0; i < size; i++)
    {
        real[i] = samples[i];
        imag[i] = 0.0;
    }

    ArduinoFFT<double> fft(real, imag, size, sampleRateHz);
    fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    fft.compute(FFTDirection::Forward);
    fft.complexToMagnitude();

    // Skip bin 0 (DC component)
    double maxMag = 0.0;
    int dominantBin = 1;
    for (int i = 1; i < size / 2; i++)
    {
        if (real[i] > maxMag)
        {
            maxMag = real[i];
            dominantBin = i;
        }
    }

    float freqResolution = sampleRateHz / size;
    float dominantFreq = dominantBin * freqResolution;

    delete[] real;
    delete[] imag;

    FFTResult result;
    result.dominantFrequency = dominantFreq;
    result.optimalSampleRate = 2.0f * dominantFreq;
    return result;
}

void printTopBins(const float* samples, int size, float sampleRateHz, int topN)
{
    double* real = new double[size];
    double* imag = new double[size];

    for (int i = 0; i < size; i++)
    {
        real[i] = samples[i];
        imag[i] = 0.0;
    }

    ArduinoFFT<double> fft(real, imag, size, sampleRateHz);
    fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    fft.compute(FFTDirection::Forward);
    fft.complexToMagnitude();

    float freqResolution = sampleRateHz / size;

    Serial0.printf("Top %d frequency bins:\n", topN);
    for (int t = 0; t < topN; t++)
    {
        double maxMag = 0.0;
        int maxBin = 0;
        for (int i = 1; i < size / 2; i++)
        {
            if (real[i] > maxMag)
            {
                maxMag = real[i];
                maxBin = i;
            }
        }
        Serial0.printf("  bin %d: %.3f Hz, magnitude %.2f\n",
                       maxBin, maxBin * freqResolution, (float)maxMag);
        real[maxBin] = 0.0;
    }

    delete[] real;
    delete[] imag;
}
