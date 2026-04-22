// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenEXR Project.

#undef __THROW
#include <math.h>
#include <cstdio>

float
divide (float x, float y)
{
    // Use stdio, not iostream, around FP traps: throwing from a SIGFPE
    // handler is not standard C++, and libstdc++ ostreams can be unsafe
    // immediately afterward (e.g. i686 + ASan CI).
    std::printf ("%g / %g\n", static_cast<double> (x), static_cast<double> (y));
    return x / y;
}

float
root (float x)
{
    std::printf ("sqrt (%g)\n", static_cast<double> (x));
    return sqrt (x);
}

float
grow (float x, int y)
{
    std::printf ("grow (%g, %d)\n", static_cast<double> (x), y);

    for (int i = 0; i < y; i++)
        x = x * x;

    return x;
}
