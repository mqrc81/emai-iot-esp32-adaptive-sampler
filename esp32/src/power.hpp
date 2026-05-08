#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

struct PowerReading {
    float currentMa;
    float voltageV;
    float powerMw;
    float energyMj; // accumulated since last resetEnergy() call
    char phase[32];
};

bool powerInit();

void powerResetEnergy();

PowerReading powerRead(const char *phase);
