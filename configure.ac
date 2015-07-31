# GtkTerm autoconf script

AC_INIT([GtkTerm], [1.0], [https://fedorahosted.org/gtkterm])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])

GETTEXT_PACKAGE="gtkterm"
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])
IT_PROG_INTLTOOL([0.40.0])
AM_GLIB_GNU_GETTEXT

# Use AM_SILENT_RULES if present
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

PKG_CHECK_MODULES([gtk], [gtk+-3.0 >= 3.12.0])
PKG_CHECK_MODULES([vte], [vte-2.91 >= 0.40.0])

if test "$CC" = "gcc" ; then
  GNUCFLAGS="-Wall -fno-omit-frame-pointer -fno-strict-aliasing -O2"
fi
AC_SUBST(GNUCFLAGS)

# Bundle GTK ui files with gresource
AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources, no)
if test x$GLIB_COMPILE_RESOURCES = xno; then
	AC_MSG_ERROR(Could not find glib-compile-resources in your PATH)
fi

# Linux is required for arbitrary baud rates
AC_CHECK_HEADERS_ONCE([linux/serial.h])

AC_CONFIG_FILES([Makefile src/Makefile po/Makefile.in])

AC_OUTPUT
