AC_INIT([amlibnpcm-jtag],[1.0],[bug-automake@gnu.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_AR
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# libtool init
LT_INIT

AC_CONFIG_FILES([
 Makefile
  src/Makefile
  lib/Makefile
])

AC_ARG_ENABLE([legacy-ioctl],
              [AS_HELP_STRING([--enable-legacy-ioctl],[Use legacy ioctl])])
AS_IF([test "x$enable_legacy_ioctl" = "xyes"],
      [AC_DEFINE([USE_LEGACY_IOCTL], [1], [Define to 1 if you use legacy ioctl])],
      [])

AC_ARG_ENABLE([build-loadsvf],
              [AS_HELP_STRING([--enable-build-loadsvf],[build loadsvf])])
AM_CONDITIONAL([BUILD_LOADSVF],  [test "x$enable_build_loadsvf" = "xyes"])

AC_OUTPUT
