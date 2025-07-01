#ifndef FH_LOG_LOG_H
#define FH_LOG_LOG_H

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

#ifdef FH_LOG_MODULE
	#define _FH_LOG_MODULE FH_LOG_MODULE
#else /* not FH_LOG_MODULE */
	#define _FH_LOG_MODULE
#endif /* FH_LOG_MODULE */

#define PR_DEBUG LOG_FORMAT_SIG_STR "\x5"
#define PR_INFO LOG_FORMAT_SIG_STR "\x6"
#define PR_WARN LOG_FORMAT_SIG_STR "\x7"
#define PR_ERR LOG_FORMAT_SIG_STR "\x8"
#define PR_EMERG LOG_FORMAT_SIG_STR "\x9"

/* Macro helpers for logging in different levels. */

#define fh_pr_debug(...) fh_printl (PR_DEBUG __VA_ARGS__)
#define fh_pr_info(...) fh_printl (PR_INFO __VA_ARGS__)
#define fh_pr_warn(...) fh_printl (PR_WARN __VA_ARGS__)
#define fh_pr_err(...) fh_printl (PR_ERR __VA_ARGS__)
#define fh_pr_emerg(...) fh_printl (PR_EMERG __VA_ARGS__)

#undef _FH_LOG_MODULE

#endif /* FH_LOG_LOG_H */
