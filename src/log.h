#ifndef FHTTPD_LOG_H
#define FHTTPD_LOG_H

#include <stdio.h>

enum fhttpd_log_level
{
    FHTTPD_LOG_LEVEL_DEBUG,
    FHTTPD_LOG_LEVEL_INFO,
    FHTTPD_LOG_LEVEL_WARNING,
    FHTTPD_LOG_LEVEL_ERROR,
    FHTTPD_LOG_LEVEL_FATAL
};

typedef enum fhttpd_log_level log_level_t;

void fhttpd_log(log_level_t level, const char *format, ...);

log_level_t fhttpd_log_get_level(void);
FILE *fhttpd_log_get_output(void);

void fhttpd_log_set_level(log_level_t level);
void fhttpd_log_set_output(FILE *_stderr, FILE *_stdout);

void fhttpd_perror(const char *format, ...);

#define fhttpd_log_debug(format, ...) \
    fhttpd_log(FHTTPD_LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define fhttpd_log_info(format, ...) \
    fhttpd_log(FHTTPD_LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define fhttpd_log_warning(format, ...) \
    fhttpd_log(FHTTPD_LOG_LEVEL_WARNING, format, ##__VA_ARGS__)
#define fhttpd_log_error(format, ...) \
    fhttpd_log(FHTTPD_LOG_LEVEL_ERROR, format, ##__VA_ARGS__)
#define fhttpd_log_fatal(format, ...) \
    fhttpd_log(FHTTPD_LOG_LEVEL_FATAL, format, ##__VA_ARGS__)

#define fhttpd_wlog_debug(pid, format, ...) \
    fhttpd_log(FHTTPD_LOG_LEVEL_DEBUG, "[Worker %d] " format, pid, ##__VA_ARGS__)
#define fhttpd_wlog_info(pid, format, ...) \
    fhttpd_log(FHTTPD_LOG_LEVEL_INFO, "[Worker %d] " format, pid, ##__VA_ARGS__)
#define fhttpd_wlog_warning(pid, format, ...) \
    fhttpd_log(FHTTPD_LOG_LEVEL_WARNING, "[Worker %d] " format, pid, ##__VA_ARGS__)
#define fhttpd_wlog_error(pid, format, ...) \
    fhttpd_log(FHTTPD_LOG_LEVEL_ERROR, "[Worker %d] " format, pid, ##__VA_ARGS__)
#define fhttpd_wlog_fatal(pid, format, ...) \
    fhttpd_log(FHTTPD_LOG_LEVEL_FATAL, "[Worker %d] " format, pid, ##__VA_ARGS__)

#define fhttpd_wlog_perror(pid, format, ...) \
    fhttpd_log(FHTTPD_LOG_LEVEL_ERROR, "[Worker %d] " format ": %s", pid, ##__VA_ARGS__, strerror(errno))

#define fhttpd_wclog_debug(format, ...) \
    fhttpd_wlog_debug(getpid(), format, ##__VA_ARGS__)
#define fhttpd_wclog_info(format, ...) \
    fhttpd_wlog_info(getpid(), format, ##__VA_ARGS__)
#define fhttpd_wclog_warning(format, ...) \
    fhttpd_wlog_warning(getpid(), format, ##__VA_ARGS__)
#define fhttpd_wclog_error(format, ...) \
    fhttpd_wlog_error(getpid(), format, ##__VA_ARGS__)
#define fhttpd_wclog_fatal(format, ...) \
    fhttpd_wlog_fatal(getpid(), format, ##__VA_ARGS__)

#define fhttpd_wclog_perror(format, ...) \
    fhttpd_wlog_perror(getpid(), format, ##__VA_ARGS__)

#endif /* FHTTPD_LOG_H */