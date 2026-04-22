#include <stdio.h>
#include "signal.h"

#define SAMPLE_RATE_HZ 100

int main()
{
    for (int i = 0; i < 500; i++)
    {
        float t = i / (float)SAMPLE_RATE_HZ;
        printf("%.4f,%.4f\n", t, generateSignal(t));
    }
    return 0;
}
