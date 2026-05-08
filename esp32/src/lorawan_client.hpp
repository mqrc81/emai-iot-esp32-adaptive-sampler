#pragma once
#include <Arduino.h>
#include <cstdint>

// Cayenne LPP analog input encoding
// Returns number of bytes written, -1 on error
int cayenneLPPEncode(uint8_t *buf, int bufSize, float value);

bool loraInit();

bool loraJoin();

bool loraSendSummary(float value, const char *label);
