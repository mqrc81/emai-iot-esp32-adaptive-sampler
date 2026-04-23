#pragma once

struct FilterConfig
{
    int windowSize; // W — number of samples in sliding window
    float threshold; // k — number of deviations to flag
};

struct FilterResult
{
    int flagged; // 1 if anomaly detected, 0 otherwise
    float cleaned; // original value if clean, median/mean if flagged
};

// Z-score filter: flags if |x - mean| / std > threshold
struct FilterResult zscoreFilter(const float* window, int windowSize,
                                 int centerIdx, float threshold);

// Hampel filter: flags if |x - median| > threshold * 1.4826 * MAD
struct FilterResult hampelFilter(const float* window, int windowSize,
                                 int centerIdx, float threshold);