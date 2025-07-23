#                                -*- Autoconf -*-

AC_DEFUN([CC_CHECK_VLA_SUPPORT], [
    AC_MSG_CHECKING([whether $CC supports VLAs])

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
            AC_MSG_ERROR([The C compiler $CC does not support VLAs (variable-length arrays)])
        ]
    )
])

AC_DEFUN([CCLD_CHECK_RDYNAMIC_SUPPORT], [
	AC_MSG_CHECKING([whether $LD supports -rdynamic])

	prev_ldflags="$LDFLAGS"
	LDFLAGS="$LDFLAGS -rdynamic"

	AC_LINK_IFELSE(
		[AC_LANG_PROGRAM(
			[[
				#include <stdio.h>
			]],
			[[
				puts ("Hello world");
			]]
		)],
		[AC_MSG_RESULT([yes])],
		[
			AC_MSG_RESULT([no])
			AC_MSG_ERROR([The linker $LD does not support -rdynamic])
		]
	)

	LDFLAGS="$prev_ldflags"
])
