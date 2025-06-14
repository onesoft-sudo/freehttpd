#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "utils.h"

uint64_t
get_current_timestamp (void)
{
    struct timespec now;
    clock_gettime (CLOCK_REALTIME, &now);
    return (now.tv_sec * 1000) + (now.tv_nsec / 1000000);
}

_Noreturn void
freeze (void)
{
    fprintf (stderr, "Alert: process %d will be frozen\n", getpid());
    fflush (stderr);

    while (true)
        pause();
}

char *
str_trim_whitespace (const char *str, size_t len, size_t *out_len)
{
    if (len == 0)
    {
        *out_len = 0;
        return strdup("");
    }

    size_t start = 0, end = len - 1;

    while (start < len && isspace (str[start]))
        start++;
    
    while (end > 0 && isspace (str[end]))
        end--;

    if (start > end)
    {
        *out_len = 0;
        return strdup("");
    }

    size_t trimmed_len = end - start + 1;
    char *trimmed_str = malloc(trimmed_len + 1);

    if (!trimmed_str)
    {
        *out_len = 0;
        return NULL;
    }

    memcpy(trimmed_str, str + start, trimmed_len);
    trimmed_str[trimmed_len] = 0;
    *out_len = trimmed_len;

    return trimmed_str;
}