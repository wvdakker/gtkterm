# GtkTerm src/Makefile.am

AM_CFLAGS = $(gtk_CFLAGS) $(vte_CFLAGS)

bin_PROGRAMS = gtkterm

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
    logging.h 

gtkterm_LDADD = $(gtk_LIBS) $(vte_LIBS)

CLEANFILES = *~


AM_CPPFLAGS = -DLOCALEDIR=\""$(localedir)"\"
