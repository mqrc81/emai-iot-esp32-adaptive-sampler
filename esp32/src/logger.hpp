#pragma once
#include <Arduino.h>
#include <freertos/semphr.h>
#include <cstdarg>
#include <cstdio>

extern SemaphoreHandle_t serialMutex;

inline void logMsg(const char *msg) {
    if (serialMutex && xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100))) {
        Serial.println(msg);
        xSemaphoreGive(serialMutex);
    }
}

inline void logFmt(const char *fmt, ...) {
    if (serialMutex && xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100))) {
        char buf[256];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        Serial.println(buf);
        xSemaphoreGive(serialMutex);
    }
}
