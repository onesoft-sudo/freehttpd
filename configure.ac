#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([freehttpd], [1.0.0-alpha.2], [bug-freehttpd@lists.onesoftnet.eu.org])
AC_CONFIG_SRCDIR([src/freehttpd.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall gnu 1.11 subdir-objects])
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC
gl_EARLY

# Check for C23 support.
c23check_done=0

AC_MSG_CHECKING([for GNU C23 support])

saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -std=gnu23"

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
  #if __STDC_VERSION__ < 202311L
  #error Not C23
  #endif

  #if !defined(__GNUC__) || !defined(__GNUC_MINOR__) || !defined(__GNUC_PATCHLEVEL__)
  #error Not GNU C
  #endif
]], [[]])], [
    AC_MSG_RESULT([yes])
    AC_SUBST([CSTD], [gnu23])
    c23check_done=1
], [])

CFLAGS="$saved_CFLAGS"


if test "c23check_done" = "0"; then
    AC_MSG_CHECKING([for GNU C2x support])

    saved_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS -std=gnu2x"

    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
        #if __STDC_VERSION__ < 202301L
        #error Not C2x
        #endif

        #if !defined(__GNUC__) || !defined(__GNUC_MINOR__) || !defined(__GNUC_PATCHLEVEL__)
        #error Not GNU C
        #endif
    ]], [[]])], [
        AC_MSG_RESULT([yes])
        AC_SUBST([CSTD], [gnu2x])
        c23check_done=1
    ], [
        AC_MSG_ERROR([Your compiler does not support GNU C23])
    ])

    CFLAGS="$saved_CFLAGS"
fi

AM_PROG_AR
AC_PROG_RANLIB

gl_INIT

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h netdb.h netinet/in.h stdint.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
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
                 gnulib/Makefile
                 res/Makefile
                 src/Makefile
                 tests/Makefile])
AC_OUTPUT
