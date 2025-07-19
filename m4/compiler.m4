AC_DEFUN([STDC_CHECK_VLA_SUPPORT], [
    AC_MSG_CHECKING([whether the C compiler supports VLAs])
    
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
])