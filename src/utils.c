#include <time.h>

#include "utils.h"

uint64_t
utils_get_current_timestamp (void)
{
    struct timespec now;
    clock_gettime (CLOCK_REALTIME, &now);
    return (now.tv_sec * 1000) + (now.tv_nsec / 1000000);
}