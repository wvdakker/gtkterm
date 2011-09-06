# GtkTerm src/Makefile.am

AM_CFLAGS = $(gtk2_CFLAGS) $(vte_CFLAGS)

bin_PROGRAMS = gtkterm
gtkterm_SOURCES = \
    term_config.c \
    fichier.c \
    gtkterm.c \
    serie.c \
    config.h \
    fichier.h \
    serie.h \
    widgets.c \
    widgets.h \
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

gtkterm_LDADD = $(gtk2_LIBS) $(vte_LIBS)

CLEANFILES = *~


AM_CPPFLAGS = -DLOCALEDIR=\""$(localedir)"\"
