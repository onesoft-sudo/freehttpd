#ifndef FH_LOG_LOG_H
#define FH_LOG_LOG_H

#include <sys/types.h>

#define LOG_FORMAT_SIG 0xf2
#define LOG_FORMAT_SIG_STR "\xf2"

enum fh_log_level
{
	LOG_DEBUG = 0x5,
	LOG_INFO,
	LOG_WARN,
	LOG_ERR,
	LOG_EMERG,
};

__attribute__ ((format (printf, 1, 2))) int fh_printl (const char *format, ...);
void fh_log_set_worker_pid (pid_t pid);

#define FH_LOG_USE_COLORS

#ifdef FH_LOG_MODULE_NAME
	#ifdef FH_LOG_USE_COLORS
		#define _FH_LOG_MODULE "\033[33m" FH_LOG_MODULE_NAME ":\033[0m "
	#else
		#define _FH_LOG_MODULE FH_LOG_MODULE_NAME ": "
	#endif
#else /* not FH_LOG_MODULE */
	#define _FH_LOG_MODULE
#endif /* FH_LOG_MODULE */

#define PR_DEBUG LOG_FORMAT_SIG_STR "\x5"
#define PR_INFO LOG_FORMAT_SIG_STR "\x6"
#define PR_WARN LOG_FORMAT_SIG_STR "\x7"
#define PR_ERR LOG_FORMAT_SIG_STR "\x8"
#define PR_EMERG LOG_FORMAT_SIG_STR "\x9"

/* Macro helpers for logging in different levels. */

#define fh_pr_debug(...) fh_printl (PR_DEBUG _FH_LOG_MODULE __VA_ARGS__)
#define fh_pr_info(...) fh_printl (PR_INFO _FH_LOG_MODULE __VA_ARGS__)
#define fh_pr_warn(...) fh_printl (PR_WARN _FH_LOG_MODULE __VA_ARGS__)
#define fh_pr_err(...) fh_printl (PR_ERR _FH_LOG_MODULE __VA_ARGS__)
#define fh_pr_emerg(...) fh_printl (PR_EMERG _FH_LOG_MODULE __VA_ARGS__)

#endif /* FH_LOG_LOG_H */
