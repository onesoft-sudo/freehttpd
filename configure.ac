dnl                                               -*- Autoconf -*-
dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([freehttpd], [1.0.0-alpha.2], [bug-freehttpd@lists.onesoftnet.eu.org])
AC_CONFIG_SRCDIR([src/freehttpd.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall gnu 1.11 subdir-objects])
AM_SILENT_RULES([yes])

dnl Checks for programs.
AC_PROG_CC

dnl Check for C23 support.
dnl c23check_done=0
dnl
dnl AC_MSG_CHECKING([whether the compiler supports GNU C23])
dnl
dnl saved_CFLAGS="$CFLAGS"
dnl CFLAGS="$CFLAGS -std=gnu23"
dnl
dnl AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
dnl   #if __STDC_VERSION__ < 202301L
dnl   #error Not C23
dnl   #endif
dnl ]], [[]])], [
dnl     AC_MSG_RESULT([yes])
dnl     AC_SUBST([CSTD], [gnu23])
dnl     c23check_done=1
dnl ], [
dnl     AC_MSG_RESULT([no])
dnl ])
dnl
dnl CFLAGS="$saved_CFLAGS"
dnl
dnl if test "$c23check_done" = "0"; then
dnl     AC_MSG_CHECKING([whether the compiler supports GNU C2x])
dnl
dnl     saved_CFLAGS="$CFLAGS"
dnl     CFLAGS="$CFLAGS -std=gnu2x"
dnl
dnl     AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
dnl         #if __STDC_VERSION__ < 202001L
dnl         #error Not C2x
dnl         #endif
dnl     ]], [[]])], [
dnl         AC_MSG_RESULT([yes])
dnl         AC_SUBST([CSTD], [gnu2x])
dnl         c23check_done=1
dnl     ], [
dnl         AC_MSG_RESULT([no])
dnl         AC_MSG_ERROR([Your compiler does not support GNU C23/C2x. Please use a compiler that supports it, such as GCC 14 or later.])
dnl     ])
dnl
dnl     CFLAGS="$saved_CFLAGS"
dnl fi

AM_PROG_AR
AC_PROG_RANLIB

dnl Checks for libraries.

dnl Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h netdb.h netinet/in.h stdint.h sys/socket.h sys/time.h unistd.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

dnl Checks for library functions.
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([atexit clock_gettime gettimeofday memchr memmove memset socket strcasecmp strdup strerror strndup strrchr strtoull])

AC_CONFIG_FILES([Makefile
                 res/Makefile
                 src/Makefile
                 tests/Makefile])
AC_OUTPUT
