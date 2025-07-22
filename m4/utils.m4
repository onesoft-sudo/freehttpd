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
  Main configuration file: $main_config_file_path
  Module path: $module_dir_path
  Optional systemd support: $enable_systemd
  Optional modules: $all_modules
  Optimizations: $enable_optimizations
	])
])

AC_DEFUN([DEFINE_VERSION], [
    define(
        [FHTTPD_VERSION],
        m4_esyscmd([
            if test -f "$srcdir/.tarball-version"; then
            head -n1 "$srcdir/.tarball-version" | tr -d '\n';
            else
                jq -r '.version' .github/version.json | tr -d '\n';
            fi
        ])
    )
])