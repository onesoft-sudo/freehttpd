#ifndef FHTTPD_COMPAT_H
#define FHTTPD_COMPAT_H

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#	ifdef HAVE_STDNORETURN_H
#		include <stdnoreturn.h>
#	endif /* HAVE_STDNORETURN_H */

#	define __noreturn _Noreturn
#else /* defined (__STDC_VERSION__) && __STDC_VERSION__ >= 201112L */
#	define __noreturn __attribute__ ((noreturn))
#   undef static_assert
#	define static_assert(cond, msg)                                                                                   \
		extern int (*__Static_assert_function (void))[!!sizeof (struct { int __error_if_negative : (cond) ? 2 : -1; })]
#endif /* defined (__STDC_VERSION__) && __STDC_VERSION__ >= 201112L */

#endif /* FHTTPD_COMPAT_H */
