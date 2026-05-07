#pragma once

struct WindowResult {
    float average;
    float windowDurationMs;
    int sampleCount;
};

struct WindowResult computeWindow(float sampleRateHz, float windowSecs);
