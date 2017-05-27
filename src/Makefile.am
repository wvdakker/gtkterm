# GtkTerm src/Makefile.am

AM_CFLAGS = $(gtk_CFLAGS) $(vte_CFLAGS)

bin_PROGRAMS = gtkterm

# Rules to compile GResources
resources=$(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies gresource.xml)
gresource.c: gresource.xml $(resources)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --generate-source --target=$@ gresource.xml

gresource.h: gresource.xml $(resources)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --generate-header --target=$@ gresource.xml

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

BUILT_SOURCES = \
    gresource.c \
    gresource.h

gtkterm_LDADD = $(gtk_LIBS) $(vte_LIBS)

CLEANFILES = $(BUILT_SOURCES) *~


AM_CPPFLAGS = -DLOCALEDIR=\""$(localedir)"\"
