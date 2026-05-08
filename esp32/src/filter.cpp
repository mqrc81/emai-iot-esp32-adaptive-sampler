#include "filter.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>

#define MAX_WINDOW_SIZE 201

// ======= UTILITIES =======

static float computeMean(const float *data, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; i++) sum += data[i];
    return sum / n;
}

static float computeStd(const float *data, int n, float mean) {
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        float diff = data[i] - mean;
        sum += diff * diff;
    }
    return sqrtf(sum / n);
}

static int floatCmp(const void *a, const void *b) {
    float fa = *(const float *) a;
    float fb = *(const float *) b;
    return (fa > fb) - (fa < fb);
}

static float computeMedian(const float *data, int n) {
    float sorted[MAX_WINDOW_SIZE];
    memcpy(sorted, data, n * sizeof(float));
    qsort(sorted, n, sizeof(float), floatCmp);
    return (n % 2 == 0)
               ? (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0f
               : sorted[n / 2];
}

// ======= FILTERS =======

FilterResult zscoreFilter(const float *window, int windowSize, int centerIdx, float threshold) {
    float mean = computeMean(window, windowSize);
    float std = computeStd(window, windowSize, mean);
    float x = window[centerIdx];
    float z = (std > 1e-6f) ? fabsf(x - mean) / std : 0.0f;

    FilterResult r{};
    r.flagged = (z > threshold) ? 1 : 0;
    r.cleaned = r.flagged ? mean : x;
    return r;
}

FilterResult hampelFilter(const float *window, int windowSize, int centerIdx, float threshold) {
    float median = computeMedian(window, windowSize);

    float deviations[MAX_WINDOW_SIZE];
    for (int i = 0; i < windowSize; i++)
        deviations[i] = fabsf(window[i] - median);
    float mad = computeMedian(deviations, windowSize);

    float sigma = 1.4826f * mad;
    float x = window[centerIdx];

    FilterResult r{};
    r.flagged = (sigma > 1e-6f && fabsf(x - median) > threshold * sigma) ? 1 : 0;
    r.cleaned = r.flagged ? median : x;
    return r;
}
