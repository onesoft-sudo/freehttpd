#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "log.h"

#ifdef NDEBUG
static log_level_t log_level = FHTTPD_LOG_LEVEL_INFO;
#else
static log_level_t log_level = FHTTPD_LOG_LEVEL_DEBUG;
#endif

static FILE *log_stderr = NULL;
static FILE *log_stdout = NULL;

log_level_t
fhttpd_log_get_level (void)
{
    return log_level;
}

FILE *
fhttpd_log_get_output (void)
{
    if (log_stderr && log_stdout)
        return log_stdout;

    return NULL;
}

void
fhttpd_log_set_level (log_level_t level)
{
    if (level < FHTTPD_LOG_LEVEL_DEBUG || level > FHTTPD_LOG_LEVEL_FATAL)
        return;

    log_level = level;
}

void
fhttpd_log_set_output (FILE *_stderr, FILE *_stdout)
{
    if (!_stderr || !_stdout)
        return;

    log_stderr = _stderr;
    log_stdout = _stdout;
}

static const char *
log_level_to_string (log_level_t level)
{
    switch (level)
    {
        case FHTTPD_LOG_LEVEL_DEBUG:
            return "debug";
        case FHTTPD_LOG_LEVEL_INFO:
            return "info";
        case FHTTPD_LOG_LEVEL_WARNING:
            return "warning";
        case FHTTPD_LOG_LEVEL_ERROR:
            return "error";
        case FHTTPD_LOG_LEVEL_FATAL:
            return "fatal";
        default:
            return "unknown";
    }
}

void
fhttpd_log (log_level_t level, const char *format, ...)
{
    assert (log_stderr != NULL && log_stdout != NULL
            && "Log output streams must be set before logging");

    if (level < log_level)
        return;

    FILE *output
        = (level == FHTTPD_LOG_LEVEL_FATAL || level == FHTTPD_LOG_LEVEL_ERROR
           || level == FHTTPD_LOG_LEVEL_WARNING)
              ? log_stderr
              : log_stdout;

    if (isatty (fileno (output)))
    {
        fprintf (output, "%s[fhttpd:%-5s]\033[0m ",
                level == FHTTPD_LOG_LEVEL_FATAL || level == FHTTPD_LOG_LEVEL_ERROR
                    ? "\033[1;31m"
                    : "",
                log_level_to_string (level));
    }
    else
    {
        fprintf (output, "[fhttpd:%-5s] ", log_level_to_string (level));
    }

    va_list args;
    va_start (args, format);
    vfprintf (output, format, args);
    va_end (args);

    fputc ('\n', output);
}

void
fhttpd_perror (const char *format, ...)
{
    assert (log_stderr != NULL
            && "Log stderr must be set before logging errors");

    fprintf (log_stderr, "[fhttpd:error] ");

    va_list args;
    va_start (args, format);
    vfprintf (log_stderr, format, args);
    va_end (args);

    fprintf (log_stderr, ": %s\n", strerror (errno));
}