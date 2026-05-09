#pragma once
#include <Arduino.h>
#include <freertos/semphr.h>
#include <cstdarg>
#include <cstdio>

extern SemaphoreHandle_t serialMutex;

inline void logMsg(const char *msg) {
    if (!serialMutex) {
        Serial.println(msg);
    } else if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100))) {
        Serial.println(msg);
        xSemaphoreGive(serialMutex);
    }
}

inline void logFmt(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (!serialMutex) {
        Serial.println(buf);
    } else if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100))) {
        Serial.println(buf);
        xSemaphoreGive(serialMutex);
    }
}

#ifndef HELTEC_NO_DISPLAY_INSTANCE
// display is defined in heltec_unofficial.h — included by main.cpp
// only call oledStatus from main.cpp
#include "SSD1306Wire.h"
extern SSD1306Wire display;

inline void oledStatus(int line, const char *fmt, ...) {
    char buf[22];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, line * 12, buf);
    display.display();
}

inline void oledClear() {
    display.clear();
    display.display();
}
#else
inline void oledStatus(int line, const char *fmt, ...) {} // noop
inline void oledClear() {} // noop
#endif
