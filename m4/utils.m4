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