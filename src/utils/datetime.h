#ifndef FH_UTILS_DATETIME_H
#define FH_UTILS_DATETIME_H

#include <time.h>
#include <stdint.h>

typedef uint64_t etime_t;

etime_t time_now (void);
double time_seconds_now (void);

#endif /* FH_UTILS_DATETIME_H */