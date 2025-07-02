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

define([FHTTPD_VERSION], m4_esyscmd([if test -f "$srcdir/.tarball-version"; then head -n1 "$srcdir/.tarball-version" | tr -d '\n'; else jq -r '.version' .github/version.json | tr -d '\n'; fi]))

AC_INIT([freehttpd], 
  FHTTPD_VERSION, 
  [bug-freehttpd@lists.onesoftnet.eu.org])
AC_CONFIG_SRCDIR([src/freehttpd.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall gnu 1.11 subdir-objects])
AM_SILENT_RULES([yes])

AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug], [Enable debug build mode (default: no)])],
              [build_type=debug],
              [build_type=release])

AC_ARG_ENABLE([systemd],
              [AS_HELP_STRING([--enable-systemd], [Enable systemd support (default: no)])],
              [enable_systemd="$enableval"],
              [enable_systemd=no])

AC_ARG_WITH([config-file], 
            [AS_HELP_STRING([--with-config-file=FILE], [Path to the main configuration file (default: $sysconfdir/freehttpd/fhttpd.conf)])],
            [FHTTPD_MAIN_CONFIG_FILE="$withval"],
            [FHTTPD_MAIN_CONFIG_FILE="$sysconfdir/freehttpd/fhttpd.conf"])

if test "x$FHTTPD_CONFIGURE_DEBUG" = "x1"; then
    AC_MSG_NOTICE([Configuring OSN freehttpd])
    AC_MSG_NOTICE([Version: ]FHTTPD_VERSION)
fi

# Checks for programs.
AC_PROG_CC

AM_PROG_AR
AC_PROG_RANLIB

AC_CHECK_PROGS([GPG], [gpg gpg2], [no])

if test "x$GPG" = "xno"; then
  GPG=""
fi

AC_SUBST([GPG])

# Checks for libraries.

if test "x$enable_systemd" = "xyes"; then
    PKG_CHECK_MODULES([SYSTEMD], [libsystemd], [], [
        AC_MSG_ERROR([libsystemd is required for systemd support. Please install it or disable systemd support with --disable-systemd.])
    ])

    AC_MSG_CHECKING([for systemd unit directory])
    systemdsystemunitdir="$libdir/systemd/system"
    AC_SUBST([systemdsystemunitdir])
    AC_MSG_RESULT([$systemdsystemunitdir])

    AC_DEFINE_UNQUOTED([FHTTPD_ENABLE_SYSTEMD], [1], [Enables systemd support])    
fi

AM_CONDITIONAL([ENABLE_SYSTEMD], [test "x$enable_systemd" = "xyes"])

AC_SUBST([SYSTEMD_CFLAGS])
AC_SUBST([SYSTEMD_LIBS])

# Define macros
AC_SUBST([FHTTPD_MAIN_CONFIG_FILE])

ABS_SRCDIR=`cd "$srcdir" && pwd`
ABS_BUILDDIR=`pwd`

AC_SUBST([ABS_SRCDIR])
AC_SUBST([ABS_BUILDDIR])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h netdb.h netinet/in.h stdint.h sys/socket.h sys/time.h unistd.h])
AC_CHECK_HEADERS([stdnoreturn.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_MSG_CHECKING([whether the compiler supports VLAs])

AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
        [[
            #ifdef __STDC_NO_VLA__
            #error "No VLA support"
            #endif
        ]]
    )],
    [AC_MSG_RESULT([yes])],
    [
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([The compiler does not support VLAs (variable-length arrays)])
    ]
)

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

AC_CONFIG_FILES([Makefile
                 res/Makefile
                 src/Makefile
                 src/core/Makefile
                 src/event/Makefile
                 src/include/Makefile
                 src/log/Makefile
                 src/utils/Makefile
                 conf/Makefile
                 systemd/Makefile
                 tests/Makefile])
AC_OUTPUT

AC_MSG_NOTICE([Configuration summary:

  Version: $VERSION
  Build type: $build_type
  Compiler: $CC
  Compiler flags: $CFLAGS $SYSTEMD_CFLAGS
  Linker flags: $LDFLAGS $SYSTEMD_LDLAGS
  Preprocessor flags: $CPPFLAGS
  Libraries: $LIBS $SYSTEMD_LIBS
  Installation prefix: $prefix
  Main Configuration file: $FHTTPD_MAIN_CONFIG_FILE
  systemd support: $enable_systemd
])
