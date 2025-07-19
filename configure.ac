#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
# This file is part of OSN freehttpd.
#
# Copyright (C) 2025  OSN Developers.
#
# OSN freehttpd is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# OSN freehttpd is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with OSN freehttpd.  If not, see <https://www.gnu.org/licenses/>.

AC_PREREQ([2.61])

define([FHTTPD_VERSION],
	m4_esyscmd([
		if test -f "$srcdir/.tarball-version"; then
		   head -n1 "$srcdir/.tarball-version" | tr -d '\n';
		else
			jq -r '.version' .github/version.json | tr -d '\n';
		fi]))

AC_INIT([freehttpd],
  FHTTPD_VERSION,
  [bug-freehttpd@lists.onesoftnet.eu.org])
AC_CONFIG_SRCDIR([src/freehttpd.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall gnu 1.11 info-in-builddir subdir-objects])
AM_SILENT_RULES([yes])

# Options
DEFINE_OPTIONS

# Checks for programs.
AC_PROG_CC
AM_PROG_AR
AC_PROG_RANLIB

AC_CHECK_PROGS([GPG], [gpg gpg2], [no])
AS_IF([test "x$GPG" = "xno"], [GPG=""], [])
AC_SUBST([GPG])

AC_CHECK_PROGS([CURL], [curl], [no])
AC_CHECK_PROGS([WGET], [wget], [no])

AS_IF([test "x$enable_rapidhash" = "x1"], [
    AS_IF([test "x$CURL" = "xno" && test "x$WGET" = "xno"], [
        AC_MSG_ERROR([curl or wget is required to fetch external files into the source tree])
        exit 1
    ])

    AC_DEFINE_UNQUOTED([HAVE_RAPIDHASH_H], [1], [Define to 1 if rapidhash algorithm is enabled])
])

AM_CONDITIONAL([ENABLE_RAPIDHASH], [test "$enable_rapidhash" = "1"])

AC_SUBST([WGET])
AC_SUBST([CURL])

# Checks for libraries.

AS_IF([test "x$enable_systemd" = "xyes"], [
    PKG_CHECK_MODULES([SYSTEMD], [libsystemd], [], [
        AC_MSG_ERROR([libsystemd is required for systemd support. Please install it or disable systemd support with --disable-systemd.])
    ])

    AC_MSG_CHECKING([for systemd unit directory])

    systemdsystemunitdir="$libdir/systemd/system"
    AC_SUBST([systemdsystemunitdir])
    AC_MSG_RESULT([$systemdsystemunitdir])

    AC_DEFINE_UNQUOTED([FHTTPD_ENABLE_SYSTEMD], [1], [Enables systemd support])

    CFLAGS="$CFLAGS $SYSTEMD_CFLAGS"
    LIBS="$LIBS $SYSTEMD_LIBS"
])

AM_CONDITIONAL([ENABLE_SYSTEMD], [test "x$enable_systemd" = "xyes"])

AC_SUBST([SYSTEMD_CFLAGS])
AC_SUBST([SYSTEMD_LIBS])
AC_SUBST([FHTTPD_MAIN_CONFIG_FILE])

ABS_SRCDIR=`cd "$srcdir" && pwd`
ABS_BUILDDIR=`pwd`

AC_SUBST([ABS_SRCDIR])
AC_SUBST([ABS_BUILDDIR])

# Define macros

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h netdb.h netinet/in.h stdint.h sys/socket.h sys/time.h unistd.h])
AC_CHECK_HEADERS([stdnoreturn.h])

# Checks for typedefs, structures, and compiler characteristics.
STDC_CHECK_VLA_SUPPORT

AC_CHECK_HEADER_STDBOOL
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([atexit clock_gettime gettimeofday memchr memmove memset socket strcasecmp strdup strerror strndup strrchr strtoull])

AC_CONFIG_COMMANDS([fetchfiles], [
    AS_IF([test "$enable_rapidhash" = "1" && ! test -f src/hash/rapidhash.h], [
        NETWORK_FETCH([https://raw.githubusercontent.com/Nicoshev/rapidhash/refs/heads/master/rapidhash.h], [src/hash/rapidhash.h])
    ])
], [
   enable_rapidhash=$enable_rapidhash
   CURL="$CURL"
   WGET="$WGET"
])

AC_CONFIG_FILES([Makefile
                 res/Makefile
                 src/Makefile
                 src/core/Makefile
                 src/event/Makefile
                 src/hash/Makefile
                 src/mm/Makefile
                 src/http/Makefile
                 src/digest/Makefile
                 src/include/Makefile
                 src/log/Makefile
                 src/utils/Makefile
                 src/router/Makefile
                 src/modules/Makefile
                 doc/Makefile
                 man/Makefile
                 conf/Makefile
                 systemd/Makefile
                 tests/Makefile])
AC_OUTPUT

AC_MSG_NOTICE([Configuration summary:

  Version: $VERSION
  Build type: $build_type
  Compiler: $CC
  Compiler flags: $CFLAGS
  Linker flags: $LDFLAGS
  Preprocessor flags: $CPPFLAGS
  Libraries: $LIBS $SYSTEMD_LIBS
  Installation prefix: $prefix
  Main Configuration file: $FHTTPD_MAIN_CONFIG_FILE
  Optional systemd support: $enable_systemd
])
