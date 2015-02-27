dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.65)
AC_INIT(zindex, 1.0, [matt@godbolt.org])
AC_CONFIG_SRCDIR([src/zindex.cpp])
AC_CONFIG_MACRO_DIR([scripts])
AM_INIT_AUTOMAKE([foreign])

AC_PROG_CC
AX_CHECK_ZLIB
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX_11([noext])

AC_CHECK_HEADERS([stdlib.h])

AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

AC_FUNC_ERROR_AT_LINE
AC_CHECK_FUNCS([memset memcpy])

AC_CONFIG_FILES(Makefile src/Makefile)
AC_OUTPUT
