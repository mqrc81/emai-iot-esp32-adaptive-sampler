#include "filter.hpp"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

// --- Utility functions ---

static float computeMean(const float* data, int n)
{
    float sum = 0.0f;
    for (int i = 0; i < n; i++) sum += data[i];
    return sum / n;
}

static float computeStd(const float* data, int n, float mean)
{
    float sum = 0.0f;
    for (int i = 0; i < n; i++)
    {
        float diff = data[i] - mean;
        sum += diff * diff;
    }
    return sqrtf(sum / n);
}

// Comparison function for qsort
static int floatCmp(const void* a, const void* b)
{
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

static float computeMedian(float* data, int n)
{
    // Work on a copy to avoid modifying the original window
    float* sorted = (float*)malloc(n * sizeof(float));
    memcpy(sorted, data, n * sizeof(float));
    qsort(sorted, n, sizeof(float), floatCmp);
    float median = (n % 2 == 0)
                       ? (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0f
                       : sorted[n / 2];
    free(sorted);
    return median;
}

// --- Filter implementations ---

FilterResult zscoreFilter(const float* window, int windowSize,
                          int centerIdx, float threshold)
{
    float mean = computeMean(window, windowSize);
    float std = computeStd(window, windowSize, mean);

    float x = window[centerIdx];
    float z = (std > 1e-6f) ? fabsf(x - mean) / std : 0.0f;

    FilterResult result;
    result.flagged = (z > threshold) ? 1 : 0;
    result.cleaned = result.flagged ? mean : x;
    return result;
}

FilterResult hampelFilter(const float* window, int windowSize,
                          int centerIdx, float threshold)
{
    // Copy window for median computation
    float* buf = (float*)malloc(windowSize * sizeof(float));
    memcpy(buf, window, windowSize * sizeof(float));

    float median = computeMedian(buf, windowSize);

    // Compute MAD: median of |x_i - median|
    float* deviations = (float*)malloc(windowSize * sizeof(float));
    for (int i = 0; i < windowSize; i++)
        deviations[i] = fabsf(window[i] - median);
    float mad = computeMedian(deviations, windowSize);

    // 1.4826 makes MAD a consistent estimator of σ for Gaussian distributions
    float sigma = 1.4826f * mad;
    float x = window[centerIdx];

    FilterResult result;
    result.flagged = (sigma > 1e-6f && fabsf(x - median) > threshold * sigma) ? 1 : 0;
    result.cleaned = result.flagged ? median : x;

    free(buf);
    free(deviations);
    return result;
}