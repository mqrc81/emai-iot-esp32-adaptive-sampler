#include "signal.h"
#include <math.h>

#define TWO_PI_F (2.0f * M_PI)

float generateSignal(float t)
{
    return 2.0f * sinf(TWO_PI_F * 3.0f * t) +
        4.0f * sinf(TWO_PI_F * 5.0f * t);
}
