/*
 * This file is part of OSN freehttpd.
 *
 * Copyright (C) 2025  OSN Developers.
 *
 * OSN freehttpd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OSN freehttpd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with OSN freehttpd.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef FH_COMPAT_H
#define FH_COMPAT_H

#include <stdlib.h>

#if !defined(__attribute_maybe_unused__)
	#if defined(__GNUC__) || defined(__clang__)
		#define __attribute_maybe_unused__ __attribute__ ((maybe_unused))
	#else /* not (defined (__GNUC__) || defined (__clang__)) */
		#define __attribute_maybe_unused__
	#endif /* defined (__GNUC__) || defined (__clang__) */
#endif	   /* !defined (__attribute_maybe_unused__) */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
	#ifdef HAVE_STDNORETURN_H
		#include <stdnoreturn.h>
	#endif /* HAVE_STDNORETURN_H */

	#define _noreturn _Noreturn
#else /* defined (__STDC_VERSION__) && __STDC_VERSION__ >= 201112L */
	#define _noreturn __attribute__ ((noreturn))
	#undef static_assert
	#define static_assert(cond, msg)                                                                                   \
		extern int (*__Static_assert_function (void))[!!sizeof (struct { int __error_if_negative : (cond) ? 2 : -1; })]
#endif /* defined (__STDC_VERSION__) && __STDC_VERSION__ >= 201112L */

#if defined(__GNUC__) || defined(__clang__)
	#define likely(x) __builtin_expect (!!(x), 1)
	#define unlikely(x) __builtin_expect (!!(x), 0)
#else
	#define likely(x) (x)
	#define unlikely(x) (x)
#endif

#endif /* FH_COMPAT_H */
