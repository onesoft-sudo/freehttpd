#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([freehttpd], [1.0.0-alpha.2], [bug-freehttpd@lists.onesoftnet.eu.org])
AC_CONFIG_SRCDIR([src/freehttpd.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall gnu 1.11 subdir-objects])
AM_SILENT_RULES([yes])

AC_ARG_ENABLE([systemd],
              [AS_HELP_STRING([--enable-systemd], [Enable systemd support (default: no)])],
              [enable_systemd=$enableval],
              [enable_systemd=no])

AC_ARG_WITH([systemd-unitdir],
            [AS_HELP_STRING([--with-systemd-unitdir=DIR], [Directory for systemd unit files (default: /lib/systemd/system)])],
            [systemd_unitdir=$withval],
            [systemd_unitdir="/lib/systemd/system"])

# Checks for programs.
AC_PROG_CC

AM_PROG_AR
AC_PROG_RANLIB

# Checks for libraries.

if test "x$enable_systemd" = "xyes"; then
  PKG_CHECK_MODULES([SYSTEMD], [libsystemd], [], [
    AC_MSG_ERROR([libsystemd is required for systemd support. Please install it or disable systemd support with --disable-systemd.])
  ])
fi

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
                 res/Makefile
                 src/Makefile
                 tests/Makefile])
AC_OUTPUT

AC_MSG_NOTICE([Configuration summary:
  Build type: $build_type
  Compiler: $CC
  Compiler flags: $CFLAGS
  Linker flags: $LDFLAGS
  Libraries: $LIBS
  Header files: $CPPFLAGS
  Installation prefix: $prefix
  Systemd support: $enable_systemd
  Systemd unit directory: $systemd_unitdir
])