#include "datetime.h"

etime_t time_now (void)
{
    struct timespec ts;
    clock_gettime (CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000UL) + (ts.tv_nsec / 1000000UL);
}

double time_seconds_now (void)
{
    struct timespec ts;
    clock_gettime (CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + ((double) ts.tv_nsec / (double) 1000000000);
}