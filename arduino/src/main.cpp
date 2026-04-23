#include <Arduino.h>
#include <Arduino.h>

#define TWO_PI_F (2.0f * M_PI)
#define SAMPLE_RATE_HZ 1000.0f

float generateSignal(float t)
{
    return 2.0f * sinf(TWO_PI_F * 3.0f * t) +
        4.0f * sinf(TWO_PI_F * 5.0f * t);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("BOOT OK");
}

void loop()
{
    static uint32_t sampleCount = 0;
    static uint64_t lastSampleTime = 0;

    uint64_t now = esp_timer_get_time();
    uint64_t interval = (uint64_t)(1000000.0f / SAMPLE_RATE_HZ);

    if (now - lastSampleTime >= interval)
    {
        lastSampleTime = now;
        float t = sampleCount / SAMPLE_RATE_HZ;
        float value = generateSignal(t);
        Serial.printf("%.4f,%.4f\n", t, value);
        sampleCount++;
    }
}
