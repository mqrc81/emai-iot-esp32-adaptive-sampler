#pragma once
#include <stdint.h>
#include "sampler.h"

struct LoRaWANConfig
{
    const char* devEUI; // Device EUI — unique device identifier in TTN
    const char* appEUI; // Application EUI — identifies the TTN application
    const char* appKey; // AES-128 encryption key for OTAA join
    uint8_t dataRate; // DR0-DR5, lower = longer range, lower throughput
    uint8_t port; // LoRaWAN port (1-223), used to route to correct handler
};

// In a real deployment this would:
// 1. Initialize SX1276 LoRa transceiver via SPI
// 2. Perform OTAA join with TTN
// 3. Encode payload in Cayenne LPP format (compact binary, TTN-native)
// 4. Transmit via LoRaWAN, respecting duty cycle limits
void lorawanPublishWindow(const LoRaWANConfig& config,
                          const WindowResult& window,
                          float windowIntervalSecs);

// Encode average as Cayenne LPP analog input (2 bytes, 0.01 resolution)
// This is the actual encoding that would be used on real hardware
int encodeCayenneLPP(uint8_t* buf, int bufSize, float value);
