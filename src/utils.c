#define _GNU_SOURCE

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <unistd.h>

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
    fprintf (stderr, "Alert: process %d will be frozen\n", getpid ());
    fflush (stderr);

    while (true)
        pause ();
}

char *
str_trim_whitespace (const char *str, size_t len, size_t *out_len)
{
    if (len == 0)
    {
        *out_len = 0;
        return strdup ("");
    }

    size_t start = 0, end = len - 1;

    while (start < len && isspace (str[start]))
        start++;

    while (end > 0 && isspace (str[end]))
        end--;

    if (start > end)
    {
        *out_len = 0;
        return strdup ("");
    }

    size_t trimmed_len = end - start + 1;
    char *trimmed_str = malloc (trimmed_len + 1);

    if (!trimmed_str)
    {
        *out_len = 0;
        return NULL;
    }

    memcpy (trimmed_str, str + start, trimmed_len);
    trimmed_str[trimmed_len] = 0;
    *out_len = trimmed_len;

    return trimmed_str;
}

bool
path_normalize (char *dest, const char *src, size_t *len_ptr)
{
    size_t len = *len_ptr;

    if (!dest || !src || len == 0 || len > PATH_MAX)
        return false;

    char buffer[len + 1];
    size_t i = 0, j = 0;

    while (i < len && j < len)
    {
        if (src[i] == 0)
            break;

        if (src[i] == '/')
        {
            if (j == 0 || buffer[j - 1] != '/')
                buffer[j++] = '/';

            i++;
            continue;
        }

        if (src[i] == '.')
        {
            if (i + 1 >= len || src[i + 1] == '/')
            {
                i += 2;
                continue;
            }
            else if (i + 1 < len && src[i + 1] == '.' && (i + 2 >= len || src[i + 2] == '/'))
            {
                if (j > 0)
                {
                    j--;

                    while (j > 0 && buffer[j - 1] != '/')
                        j--;
                }

                i += 2;

                continue;
            }
        }

        if (src[i] == '/' || src[i] == '\\')
        {
            i++;
            continue;
        }

        if (i < len && j < len)
            buffer[j++] = src[i++];
    }

    if (j == 0 || j > PATH_MAX || j > len)
        return false;

    if (j > 1 && buffer[j - 1] == '/')
        j--;

    buffer[j] = 0;
    memcpy (dest, buffer, j);
    dest[j] = 0;
    *len_ptr = j;

    return true;
}

bool
format_size (size_t size, char buf[64], size_t *num, char unit[3])
{
    if (!buf && !num && !unit)
        return false;

    const char *units[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB" };
    size_t unit_index = 0;

    while (size >= 1024 && unit_index < sizeof (units) / sizeof (units[0]) - 1)
    {
        size /= 1024;
        unit_index++;
    }

    if (num)
        *num = size;

    if (unit)
        strncpy (unit, units[unit_index], 2);

    if (buf)
        snprintf (buf, 64, "%zu%s", size, units[unit_index]);

    return true;
}

const char *
get_file_extension (const char *filename)
{
    if (!filename)
        return NULL;

    const char *dot = strrchr (filename, '.');
    
    if (!dot || dot == filename)
        return NULL;

    return dot + 1;
}