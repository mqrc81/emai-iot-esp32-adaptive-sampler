#include <stdio.h>
#include "signal.h"

#define SAMPLE_RATE_HZ 1000
#define NUM_SAMPLES    1000  // 1 second

int main()
{
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        float t = i / (float)SAMPLE_RATE_HZ;
        printf("%.4f,%.4f\n", t, generateSignal(t));
    }
    return 0;
}
