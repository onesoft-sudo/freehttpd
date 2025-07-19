AC_DEFUN([NETWORK_FETCH], [
    code=0

    AS_IF([test "x$CURL" != "xno"], [
        "$CURL" -fSsL "$1" -o "$2"
        code=$?
    ], [
        AS_IF([test "x$WGET" != "xno"], [
            "$WGET" -q "$1" -O "$2"
            code=$?
        ])
    ])

    AS_IF([test "$code" != "0"], [
        AC_MSG_ERROR([Failed to fetch required file: $1])
        exit 1
    ])
])

AC_DEFUN([PRINT_SUMMARY], [
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
])