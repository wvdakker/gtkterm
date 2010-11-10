bin_PROGRAMS = gtkterm

AM_CFLAGS = @GTK_CFLAGS@ @VTE_CFLAGS@ @GNUCFLAGS@

gtkterm_SOURCES = \
    term_config.c      \
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
    auto_config.h

gtkterm_LDADD = @GTK_LIBS@ @VTE_LIBS@

CLEANFILES = *~

INCLUDES = -DLOCALEDIR=\""$(localedir)"\"

