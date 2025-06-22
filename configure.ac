#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([freehttpd], [1.0.0-alpha.2], [bug-freehttpd@lists.onesoftnet.eu.org])
AC_CONFIG_SRCDIR([src/fhttpd.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall gnu 1.11 subdir-objects])
AM_SILENT_RULES([yes])

AC_ARG_ENABLE([systemd],
              [AS_HELP_STRING([--enable-systemd], [Enable systemd support (default: no)])],
              [enable_systemd="$enableval"],
              [enable_systemd=no])

AC_ARG_WITH([systemd-unitdir],
            [AS_HELP_STRING([--with-systemd-unitdir=DIR], [Directory for systemd unit files (default: $libdir/systemd/system)])],
            [systemd_unitdir="$withval"],
            [systemd_unitdir="$libdir/systemd/system"])

AC_ARG_WITH([config-file], 
            [AS_HELP_STRING([--with-config-file=FILE], [Path to the main configuration file (default: $sysconfdir/freehttpd/fhttpd.conf)])],
            [fhttpd_config_file="$withval"],
            [fhttpd_config_file="$sysconfdir/freehttpd/fhttpd.conf"])

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

# Define macros
AC_DEFINE_UNQUOTED([FHTTPD_MAIN_CONFIG_FILE], ["$fhttpd_config_file"], [Path to the configuration file])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h netdb.h netinet/in.h stdint.h sys/socket.h sys/time.h unistd.h])

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
  systemd support: $enable_systemd
  systemd unit directory: $systemd_unitdir
  Configuration file: $fhttpd_config_file
])
