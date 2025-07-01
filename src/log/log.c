#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "utils/datetime.h"

#ifdef NDEBUG
static enum fh_log_level min_level = LOG_INFO;
#else  /* not NDEBUG */
static enum fh_log_level min_level = LOG_DEBUG;
#endif /* NDEBUG */

static etime_t startup_time = 0;

static etime_t
now (void)
{
    struct timespec ts;
    clock_gettime (CLOCK_MONOTONIC, &ts);
    return (ts.tv_nsec / 1000) + (ts.tv_sec * 1000000);
}

__attribute__ ((constructor)) static void
fh_log_setup (void)
{
    startup_time = now ();
}

__attribute__ ((format (printf, 1, 2))) int
fh_printl (const char *format, ...)
{
	bool sig_fmt = ((uint8_t) format[0]) == LOG_FORMAT_SIG;
	enum fh_log_level level = sig_fmt ? format[1] : LOG_INFO;

	if (level < min_level)
		return 0;

	FILE *stream = level >= LOG_WARN ? stderr : stdout;

	fprintf (stream, "[%12.7lf] [%s] ", ((double) (now () - startup_time)) / (double) 1000000,
			 level == LOG_DEBUG	 ? "debug"
			 : level == LOG_INFO ? "info"
			 : level == LOG_WARN ? "warn"
			 : level == LOG_ERR	 ? "error"
								 : "emerg");

	va_list args;
	va_start (args, format);
	int ret = vfprintf (stream, sig_fmt ? format + 2 : format, args);
	va_end (args);

	fputc ('\n', stream);
	return ret;
}
