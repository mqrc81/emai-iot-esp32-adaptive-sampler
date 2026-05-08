#pragma once
#include <Arduino.h>

struct FilterResult {
    int flagged;
    float cleaned;
};

// Z-score filter: flags if |x - mean| / std > threshold
FilterResult zscoreFilter(const float *window, int windowSize, int centerIdx, float threshold);

// Hampel filter: flags if |x - median| > threshold * 1.4826 * MAD
FilterResult hampelFilter(const float *window, int windowSize, int centerIdx, float threshold);
