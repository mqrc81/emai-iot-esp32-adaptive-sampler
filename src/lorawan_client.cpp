#include "lorawan_client.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>

int encodeCayenneLPP(uint8_t* buf, int bufSize, float value)
{
    // Cayenne LPP analog input encoding:
    // byte 0: channel (0x01)
    // byte 1: type (0x02 = analog input)
    // bytes 2-3: value * 100 as signed 16-bit big-endian integer
    if (bufSize < 4) return -1;

    int16_t encoded = (int16_t)(value * 100.0f);
    buf[0] = 0x01; // channel 1
    buf[1] = 0x02; // analog input type
    buf[2] = (encoded >> 8) & 0xFF; // MSB
    buf[3] = encoded & 0xFF; // LSB
    return 4; // bytes written
}

void lorawanPublishWindow(const LoRaWANConfig& config,
                          const WindowResult& window,
                          float windowIntervalSecs)
{
    // Encode payload
    uint8_t payload[4];
    int payloadLen = encodeCayenneLPP(payload, sizeof(payload), window.average);

    printf("[LoRaWAN STUB] Simulating TTN uplink:\n");
    printf("  DevEUI      : %s\n", config.devEUI);
    printf("  AppEUI      : %s\n", config.appEUI);
    printf("  Port        : %d\n", config.port);
    printf("  Data rate   : DR%d\n", config.dataRate);
    printf("  Payload     : ");
    for (int i = 0; i < payloadLen; i++)
        printf("%02X ", payload[i]);
    printf("(%d bytes, Cayenne LPP)\n", payloadLen);
    printf("  Decoded avg : %.6f\n", window.average);

    // Real constraints that would apply on actual hardware:
    printf("\n[LoRaWAN STUB] Real deployment notes:\n");
    printf("  Duty cycle  : 1%% max in EU868 band → max 36s TX per hour\n");
    printf("  At DR0 (SF12/BW125): ~250ms airtime per packet\n");
    printf("  At DR5 (SF7/BW125) : ~20ms airtime per packet\n");
    printf("  TTN fair use policy: max 30s uplink airtime per day\n");
    printf("  At %.0fs window interval: %.0f uplinks/day, ~%.0f ms airtime/day\n",
           windowIntervalSecs,
           86400.0f / windowIntervalSecs,
           86400.0f / windowIntervalSecs * 20.0f);
}
