dnl Process this file with autoconf to produce a configure script.
AC_INIT(src/gtkterm.c)
AM_INIT_AUTOMAKE(gtkterm, 0.99.5)
AM_CONFIG_HEADER(config.h)
ALL_LINGUAS="fr hu"

AC_PROG_CC

dnl Checks for programs.

dnl Checks for libraries.

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h unistd.h)
AM_SYS_POSIX_TERMIOS

AM_GNU_GETTEXT
localedir='${prefix}/${DATADIRNAME}/locale'
AC_SUBST(localedir)

dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for library functions.

dnl Checks for GTK+ and Imlib and libvte
PKG_CHECK_MODULES(TERMINAL_WIDGET, vte >= 0.10.4)

dnl CFLAGS="$TERMINAL_WIDGET_CFLAGS $CFLAGS"

AM_PATH_GTK_2_0(2.0.0, , AC_MSG_ERROR([*** GTK >= 2.0 not installed ***]))
CPPFLAGS="$CPPFLAGS $GTK_CFLAGS"
LIBS="$LIBS $GTK_LIBS $TERMINAL_WIDGET_LIBS"

if test "$CC" = "gcc" ; then
  GNUCFLAGS="-Wall"
fi
AC_SUBST(GNUCFLAGS)

AC_OUTPUT([ po/Makefile.in m4/Makefile  intl/Makefile 
Makefile
src/Makefile
])
