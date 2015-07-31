# GtkTerm src/Makefile.am

AM_CFLAGS = $(gtk_CFLAGS) $(vte_CFLAGS)

bin_PROGRAMS = gtkterm

# Rules to compile GResources
gresource.c: gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --generate-header --target=gresource.h gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --generate-source --target=gresource.c gresource.xml

.PHONY: gresource.c

gtkterm_SOURCES = \
    term_config.c \
    files.c \
    gtkterm.c \
    serial.c \
    config.h \
    files.h \
    serial.h \
    interface.c \
    interface.h \
    cmdline.c \
    cmdline.h \
    parsecfg.c \
    parsecfg.h \
    buffer.c \
    buffer.h \
    macros.c \
    macros.h \
    i18n.c \
    i18n.h \
    auto_config.h \
    logging.c \
    logging.h \
	gresource.c \
	gresource.h

gtkterm_LDADD = $(gtk_LIBS) $(vte_LIBS)

CLEANFILES = *~


AM_CPPFLAGS = -DLOCALEDIR=\""$(localedir)"\"
