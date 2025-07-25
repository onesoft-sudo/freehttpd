#                 -*- Autoconf -*-

AC_DEFUN([CHECK_PROG_GPG], [
    AC_CHECK_PROGS([GPG], [gpg gpg2], [no])

	AS_IF([test "x$GPG" = "xno"], [GPG=""], [])
	AC_SUBST([GPG])
])

AC_DEFUN([CHECK_PROG_NETWORK_FETCH], [
    AC_CHECK_PROGS([CURL], [curl], [no])
	AC_CHECK_PROGS([WGET], [wget], [no])

	AC_SUBST([WGET])
	AC_SUBST([CURL])
])
