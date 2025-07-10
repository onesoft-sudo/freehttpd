#include "calc.h"

uint64_t 
powull (uint64_t base, uint64_t exp)
{
    uint64_t result = 1;

    while (exp)
    {
        if (exp & 1)
            result *= base;

        exp >>= 1;
        base *= base;
    }

    return result;
}