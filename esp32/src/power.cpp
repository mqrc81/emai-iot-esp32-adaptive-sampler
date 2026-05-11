#include "power.hpp"
#include "logger.hpp"
#include <cstdint>
#include <cstring>
#include <freertos/semphr.h>

static Adafruit_INA219 ina219;
static float s_accumulatedEnergyMj = 0.0f;
static uint64_t s_lastReadUs = 0;
static SemaphoreHandle_t s_powerMutex = nullptr;

bool powerInit() {
    s_powerMutex = xSemaphoreCreateMutex();
    if (!s_powerMutex) {
        logMsg("[PWR] mutex creation failed");
        return false;
    }
    if (!ina219.begin(&Wire1)) {
        // using Wire1 to not conflict with internal display wiring
        logMsg("[PWR] INA219 not found");
        return false;
    }
    ina219.setCalibration_32V_2A();
    s_lastReadUs = esp_timer_get_time();
    logMsg("[PWR] INA219 initialised");
    return true;
}

void powerResetEnergy() {
    if (xSemaphoreTake(s_powerMutex, pdMS_TO_TICKS(100))) {
        s_accumulatedEnergyMj = 0.0f;
        s_lastReadUs = esp_timer_get_time();
        xSemaphoreGive(s_powerMutex);
    }
}

PowerReading powerRead(const char *phase) {
    PowerReading r{};
    strncpy(r.phase, phase, sizeof(r.phase) - 1);
    r.phase[sizeof(r.phase) - 1] = '\0'; // TODO: move phase out of powerRead() into WindowResult directly

    if (!xSemaphoreTake(s_powerMutex, pdMS_TO_TICKS(100)))
        return r;

    uint64_t now = esp_timer_get_time();
    float elapsedS = (now - s_lastReadUs) / 1e6f;
    s_lastReadUs = now;

    float currentMa = ina219.getCurrent_mA();
    float voltageV = ina219.getBusVoltage_V();
    float powerMw = voltageV * currentMa;

    s_accumulatedEnergyMj += powerMw * elapsedS;

    r.currentMa = currentMa;
    r.voltageV = voltageV;
    r.powerMw = powerMw;
    r.energyMj = s_accumulatedEnergyMj;

    xSemaphoreGive(s_powerMutex);
    return r;
}
