#ifndef FHTTPD_LOG_H
#define FHTTPD_LOG_H

#include <stdio.h>
#include <unistd.h>

enum fhttpd_log_level
{
	FHTTPD_LOG_LEVEL_DEBUG,
	FHTTPD_LOG_LEVEL_INFO,
	FHTTPD_LOG_LEVEL_WARNING,
	FHTTPD_LOG_LEVEL_ERROR,
	FHTTPD_LOG_LEVEL_FATAL
};

typedef enum fhttpd_log_level log_level_t;

#ifdef FHTTPD_LOG_DISABLE
#define fhttpd_log(...)
#else
void fhttpd_log (log_level_t level, const char *format, ...);
#endif

#ifndef FHTTPD_LOG_MODULE_NAME
#define _FHTTPD_LOG_MODULE_NAME ""
#else
#define _FHTTPD_LOG_MODULE_NAME FHTTPD_LOG_MODULE_NAME ": "
#endif

log_level_t fhttpd_log_get_level (void);
FILE *fhttpd_log_get_output (void);

void fhttpd_log_set_level (log_level_t level);
void fhttpd_log_set_output (FILE *_stderr, FILE *_stdout);

void fhttpd_perror (const char *format, ...);

#ifndef NDEBUG
#define fhttpd_log_debug(format, ...) fhttpd_log (FHTTPD_LOG_LEVEL_DEBUG, _FHTTPD_LOG_MODULE_NAME format, ##__VA_ARGS__)
#else
#define fhttpd_log_debug(format, ...)
#endif

#define fhttpd_log_info(format, ...) fhttpd_log (FHTTPD_LOG_LEVEL_INFO, _FHTTPD_LOG_MODULE_NAME format, ##__VA_ARGS__)
#define fhttpd_log_warning(format, ...)                                                                                \
	fhttpd_log (FHTTPD_LOG_LEVEL_WARNING, _FHTTPD_LOG_MODULE_NAME format, ##__VA_ARGS__)
#define fhttpd_log_error(format, ...) fhttpd_log (FHTTPD_LOG_LEVEL_ERROR, _FHTTPD_LOG_MODULE_NAME format, ##__VA_ARGS__)
#define fhttpd_log_fatal(format, ...) fhttpd_log (FHTTPD_LOG_LEVEL_FATAL, _FHTTPD_LOG_MODULE_NAME format, ##__VA_ARGS__)

#ifndef NDEBUG
#define fhttpd_wlog_debug(pid, format, ...)                                                                            \
	fhttpd_log (FHTTPD_LOG_LEVEL_DEBUG, "[Worker %d] " _FHTTPD_LOG_MODULE_NAME format, pid, ##__VA_ARGS__)
#else
#define fhttpd_wlog_debug(pid, format, ...)
#endif

#define fhttpd_wlog_info(pid, format, ...)                                                                             \
	fhttpd_log (FHTTPD_LOG_LEVEL_INFO, "[Worker %d] " _FHTTPD_LOG_MODULE_NAME format, pid, ##__VA_ARGS__)
#define fhttpd_wlog_warning(pid, format, ...)                                                                          \
	fhttpd_log (FHTTPD_LOG_LEVEL_WARNING, "[Worker %d] " _FHTTPD_LOG_MODULE_NAME format, pid, ##__VA_ARGS__)
#define fhttpd_wlog_error(pid, format, ...)                                                                            \
	fhttpd_log (FHTTPD_LOG_LEVEL_ERROR, "[Worker %d] " _FHTTPD_LOG_MODULE_NAME format, pid, ##__VA_ARGS__)
#define fhttpd_wlog_fatal(pid, format, ...)                                                                            \
	fhttpd_log (FHTTPD_LOG_LEVEL_FATAL, "[Worker %d] " _FHTTPD_LOG_MODULE_NAME format, pid, ##__VA_ARGS__)

#define fhttpd_wlog_perror(pid, format, ...)                                                                           \
	fhttpd_log (FHTTPD_LOG_LEVEL_ERROR, "[Worker %d] " format ": %s", pid, ##__VA_ARGS__, strerror (errno))

#ifndef NDEBUG
#define fhttpd_wclog_debug(format, ...) fhttpd_wlog_debug (getpid (), format, ##__VA_ARGS__)
#else
#define fhttpd_wclog_debug(format, ...)
#endif

#define fhttpd_wclog_info(format, ...) fhttpd_wlog_info (getpid (), format, ##__VA_ARGS__)
#define fhttpd_wclog_warning(format, ...) fhttpd_wlog_warning (getpid (), format, ##__VA_ARGS__)
#define fhttpd_wclog_error(format, ...) fhttpd_wlog_error (getpid (), format, ##__VA_ARGS__)
#define fhttpd_wclog_fatal(format, ...) fhttpd_wlog_fatal (getpid (), format, ##__VA_ARGS__)

#define fhttpd_wclog_perror(format, ...) fhttpd_wlog_perror (getpid (), format, ##__VA_ARGS__)

#endif /* FHTTPD_LOG_H */
