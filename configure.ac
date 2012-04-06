# GtkTerm autoconf script

AC_INIT([GtkTerm], [1.0], [https://fedorahosted.org/gtkterm])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

# Use AM_SILENT_RULES if present
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

PKG_CHECK_MODULES([gtk], [gtk+-3.0 >= 3.0.0])
#PKG_CHECK_MODULES([vte], [vte-2.90 >= 0.30.0])
PKG_CHECK_MODULES([vte], [vte-2.90 >= 0.28.0])

if test "$CC" = "gcc" ; then
  GNUCFLAGS="-Wall -fno-omit-frame-pointer -fno-strict-aliasing -O2"
fi
AC_SUBST(GNUCFLAGS)

AC_OUTPUT
