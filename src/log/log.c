#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "compat.h"
#include "log.h"
#include "utils/datetime.h"

#ifdef NDEBUG
static enum fh_log_level min_level = LOG_INFO;
#else  /* not NDEBUG */
static enum fh_log_level min_level = LOG_DEBUG;
#endif /* NDEBUG */

static etime_t startup_time = 0;
static bool is_tty = false;
static pid_t main_pid = 0;
static bool is_master = true;

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
	main_pid = getpid ();
	startup_time = now ();
	is_tty = isatty (STDOUT_FILENO) && isatty (STDERR_FILENO);
}

void
fh_log_set_worker_pid (pid_t pid)
{
	main_pid = pid;
	is_master = false;
}

__attribute__ ((format (printf, 1, 2))) int
fh_printl (const char *format, ...)
{
	bool sig_fmt = ((uint8_t) format[0]) == LOG_FORMAT_SIG;
	enum fh_log_level level = sig_fmt ? format[1] : LOG_INFO;

	if (level < min_level)
		return 0;

	FILE *stream = level >= LOG_WARN ? stderr : stdout;

	if (likely (is_tty))
	{
		fprintf (stream, "\033[32m[%12.7lf]\033[0m \033[1m[%5s]\033[0m \033[2m[%s %d]\033[0m ",
				 ((double) (now () - startup_time)) / (double) 1000000,
				 level == LOG_DEBUG	 ? "debug"
				 : level == LOG_INFO ? "info"
				 : level == LOG_WARN ? "warn"
				 : level == LOG_ERR	 ? "error"
									 : "emerg",
				 is_master ? "Master" : "Worker", main_pid);
	}
	else
	{
		fprintf (stream, "[%12.7lf] [%s] [%s %d]", ((double) (now () - startup_time)) / (double) 1000000,
				 level == LOG_DEBUG	 ? "debug"
				 : level == LOG_INFO ? "info"
				 : level == LOG_WARN ? "warn"
				 : level == LOG_ERR	 ? "error"
									 : "emerg",
				 is_master ? "Master" : "Worker", main_pid);
	}

	va_list args;
	va_start (args, format);
	int ret = vfprintf (stream, sig_fmt ? format + 2 : format, args);
	va_end (args);

	fputc ('\n', stream);
	return ret;
}
