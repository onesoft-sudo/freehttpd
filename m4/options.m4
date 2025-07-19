AC_DEFUN([DEFINE_OPTIONS], [
    AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug], [Enable debug build mode (default: no)])],
              [build_type=debug],
              [build_type=release])

    AC_ARG_ENABLE([optimizations],
                [AS_HELP_STRING([--enable-optimizations], [Enable the highest optimization flags (default: no)])],
                [
                    : ${CFLAGS="-g -Ofast -flto -march=native -z now -Wall -Wextra -pedantic -std=gnu99"}
                    : ${LDFLAGS="-flto -march=native -z now"}
                ],
                [])

    AC_ARG_ENABLE([systemd],
                [AS_HELP_STRING([--enable-systemd], [Enable systemd support (default: no)])],
                [enable_systemd="$enableval"],
                [enable_systemd=no])

    AC_ARG_ENABLE([rapidhash],
                [AS_HELP_STRING([--enable-rapidhash], [Enable rapidhash algorithm (downloads the required header files).])],
                [enable_rapidhash=1],
                [enable_rapidhash=0])

    AC_ARG_WITH([config-file],
                [AS_HELP_STRING([--with-config-file=FILE], [Path to the main configuration file (default: $sysconfdir/freehttpd/fhttpd.conf)])],
                [FHTTPD_MAIN_CONFIG_FILE="$withval"],
                [FHTTPD_MAIN_CONFIG_FILE="$sysconfdir/freehttpd/fhttpd.conf"])

    AC_MSG_CHECKING([whether to enable debug mode])

    AS_IF([test "x$build_type" = "xdebug"], [
        AC_MSG_RESULT([yes])
    ], [
        AC_MSG_RESULT([no])
        CPPFLAGS="$CPPFLAGS -DNDEBUG"
    ])
])