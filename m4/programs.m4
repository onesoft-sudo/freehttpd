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

AC_DEFUN([CHECK_PROG_EMACS], [
	EMACS=""

	AC_MSG_CHECKING([for emacs])

	prev_ifs="$IFS"
	IFS=':'

	for path in $PATH; do
		for type in '' nox nw desktop; do
			executable="emacs"

			AS_IF([test -n "$type"], [executable="${executable}-$type"], [])
			file_path="$path/$executable"

			AS_IF([AS_EXECUTABLE_P([$file_path])], [
				EMACS="$executable"
				break
			], [
			   versioned_file="$(sh -c "ls '$path' 2>/dev/null | $EGREP '^emacs-[0-9]+\.[0-9]-$type' | head -n1")"

			   if test "$?" != "0" || test -z "$version_file"; then
			       continue
			   fi

			   AS_IF([AS_EXECUTABLE_P(["$path/$versioned_file"])], [
			       EMACS="$versioned_file"
				   break
			   ], [])
			])
		done

		AS_IF([test -n "$EMACS"], [break], [])
	done

	IFS="$prev_ifs"

	AS_IF([test -z "$EMACS"], [
		AC_MSG_RESULT([no])
		AC_MSG_ERROR([Cannot find emacs.  Please install emacs to allow processing of Org files.])
	], [
		AC_MSG_RESULT([$EMACS])
	])

	AC_SUBST([EMACS])
])
