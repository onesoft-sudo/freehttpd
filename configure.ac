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
define([FHTTPD_VERSION], m4_esyscmd([build-aux/git-version-gen -d .github/version.json .version]))

AC_INIT([freehttpd],
  FHTTPD_VERSION,
  [bug-freehttpd@lists.onesoftnet.eu.org],
  [freehttpd],
  [https://github.com/onesoft-sudo/freehttpd])

AC_CONFIG_SRCDIR([src/freehttpd.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_REQUIRE_AUX_FILE([git-version-gen])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_TARGET
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([-Wall gnu 1.11 info-in-builddir subdir-objects dist-xz dist-bzip2])
AM_SILENT_RULES([yes])

AC_SUBST([VERSION], FHTTPD_VERSION)

# Set default variables

CONCAT_SPACED_STRING([CFLAGS], [-g -O2 -std=gnu99 -Wall -Wextra -pedantic])

# Options
AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug], [Enable debug build mode (default: no)])],
              [build_type=debug],
              [build_type=release])

AC_ARG_ENABLE([optimizations],
            [AS_HELP_STRING([--enable-optimizations], [Enable the highest optimization flags (default: no)])],
            [
				CONCAT_SPACED_STRING([CFLAGS], [-Ofast -flto -march=native -z now])
				CONCAT_SPACED_STRING([LDFLAGS], [-flto -march=native -z now])

				enable_optimizations=yes
            ],
            [enable_optimizations=no])

AC_ARG_ENABLE([werror],
			[AS_HELP_STRING([--enable-werror], [Turn on the -Werror compiler flag to make every warning an error (default: no)])],
			[CONCAT_SPACED_STRING([CFLAGS], [-Werror])],
			[])

AC_ARG_ENABLE([systemd],
            [AS_HELP_STRING([--enable-systemd], [Enable systemd support (default: no)])],
            [enable_systemd="$enableval"],
            [enable_systemd=no])

AC_ARG_ENABLE([rapidhash],
            [AS_HELP_STRING([--enable-rapidhash], [Enable rapidhash algorithm (downloads the required header files, default: no).])],
            [enable_rapidhash=yes],
            [enable_rapidhash=no])

AC_ARG_ENABLE([doc],
            [AS_HELP_STRING([--enable-doc], [Enable documentation generation (default: no).])],
            [enable_doc=yes],
            [enable_doc=no])

AC_ARG_WITH([config-file],
            [AS_HELP_STRING([--with-config-file=PATH], [Path to the main configuration file (default: $sysconfdir/freehttpd/fhttpd.conf)])],
            [FHTTPD_MAIN_CONFIG_FILE="$withval"],
            [FHTTPD_MAIN_CONFIG_FILE="$sysconfdir/freehttpd/fhttpd.conf"])

AC_ARG_WITH([module-path],
            [AS_HELP_STRING([--with-module-path=PATH], [Path to the modules directory (default: $libdir/freehttpd/modules)])],
            [FHTTPD_MODULE_PATH="$withval"],
            [FHTTPD_MODULE_PATH="$libdir/freehttpd/modules"])

# Module options
m4_define([AVAILABLE_MODULES], [[foo]])

available_modules="m4_foreach([mod], [AVAILABLE_MODULES], [mod` `])"
enabled_modules=""

AC_ARG_ENABLE([mod-all],
            [AS_HELP_STRING([--enable-mod-all], [Enable all available modules (default: no).])],
            [enable_mod_all=yes],
            [enable_mod_all=no])

m4_foreach([name], [AVAILABLE_MODULES], [
	AC_ARG_ENABLE([mod-]name,
            [AS_HELP_STRING([--enable-mod-]name, [Enable the mod_]name[ module (default: no).])],
            [enable_mod_]name[=yes],
            [enable_mod_]name[=no])
])

AC_SUBST([FHTTPD_MAIN_CONFIG_FILE])
AC_SUBST([FHTTPD_MODULE_PATH])
AM_CONDITIONAL([ENABLE_DOC], [test "$enable_doc" = "yes"])

AC_MSG_CHECKING([whether to enable debug mode])

AS_IF([test "x$build_type" = "xdebug"], [
    AC_MSG_RESULT([yes])
], [
    AC_MSG_RESULT([no])
	CONCAT_SPACED_STRING([CPPFLAGS], [-DNDEBUG])
])

# Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AM_PROG_AR
AC_PROG_LN_S
AC_PROG_MKDIR_P
CHECK_PROG_GPG
CHECK_PROG_NETWORK_FETCH

LT_INIT

# Checks for libraries.

AC_CHECK_LIB([dl], [dlopen], [have_libdl=yes], [
    AC_MSG_ERROR([libdl is required for loading dynamic modules])
])

FEATURE_SYSTEMD_SUPPORT_CHECK
FEATURE_RAPIDHASH_CHECK

ABS_SRCDIR=`cd "$srcdir" && pwd`
ABS_BUILDDIR=`pwd`

AC_SUBST([ABS_SRCDIR])
AC_SUBST([ABS_BUILDDIR])

# Define macros
case "$host_os" in
  darwin*) shared_library_ext="dylib" ;;
  cygwin*|mingw*) shared_library_ext="dll" ;;
  *) shared_library_ext="so" ;;
esac

AC_DEFINE_UNQUOTED([SHARED_LIBRARY_EXTENSION], ["$shared_library_ext"], [The extension shared libraries use on this platform])

# Checks for header files.
AC_HEADER_SYS_WAIT
AC_CHECK_HEADER_STDBOOL
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h sys/socket.h sys/time.h])
AC_CHECK_HEADERS([stdnoreturn.h])

# Checks for typedefs, structures, and compiler characteristics.
CC_CHECK_VLA_SUPPORT
CCLD_CHECK_RDYNAMIC_SUPPORT
AC_C_INLINE
AC_C_CONST
AC_C_RESTRICT
AC_SYS_LARGEFILE

AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_INT16_T
AC_TYPE_INT64_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([atexit clock_gettime gettimeofday memchr memmove memset socket strcasecmp strdup strerror strndup strrchr strtoull])
AC_CHECK_FUNCS([realpath strchr strncasecmp strtoul getopt_long])

# Enable modules
m4_foreach([name], [AVAILABLE_MODULES], [
    AM_CONDITIONAL([ENABLE_MOD_]m4_toupper(name), [test "x$enable_mod_]name[" = "xyes" || test "x$enable_mod_all" = "xyes"])

	AS_IF([test "x$enable_mod_]name[" = "xyes" || test "x$enable_mod_all" = "xyes"], [
		CONCAT_SPACED_STRING([enabled_modules], [mod_]name)
		AC_MSG_NOTICE([enabling module: mod_]name)
	])
])

AC_CONFIG_COMMANDS([fetchfiles], [
    AS_IF([test "$enable_rapidhash" = "yes" && ! test -f src/hash/rapidhash.h], [
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
PRINT_SUMMARY
