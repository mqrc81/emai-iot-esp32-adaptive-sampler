#pragma once

struct WindowResult
{
    float average;
    float windowDurationMs;
    int sampleCount;
};

WindowResult computeWindow(float sampleRateHz, float windowSecs);