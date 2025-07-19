AC_DEFUN([FEATURE_SYSTEMD_SUPPORT_CHECK], [
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
])

AC_DEFUN([FEATURE_RAPIDHASH_CHECK], [
    AS_IF([test "x$enable_rapidhash" = "x1"], [
        AS_IF([test "x$CURL" = "xno" && test "x$WGET" = "xno"], [
            AC_MSG_ERROR([curl or wget is required to fetch external files into the source tree])
            exit 1
        ])

        AC_DEFINE_UNQUOTED([HAVE_RAPIDHASH_H], [1], [Define to 1 if rapidhash algorithm is enabled])
    ])

    AM_CONDITIONAL([ENABLE_RAPIDHASH], [test "$enable_rapidhash" = "1"])
])