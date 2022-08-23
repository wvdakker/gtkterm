#ifndef GTKTERM_WINDOW_H
#define GTKTERM_WINDOW_H

#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>


G_BEGIN_DECLS

#define GTKTERM_TYPE_GTKTERM_WINDOW gtkterm_window_get_type()
typedef struct _GtkTermWindow GtkTermWindow;
G_DECLARE_FINAL_TYPE (GtkTermWindow, gtkterm_window, GTKTERM, WINDOW, GtkApplicationWindow)

G_END_DECLS

void create_window (GApplication *, GtkTermWindow *);
void gtkterm_show_infobar (GtkTermWindow *, char *, int);

#endif // GTKTERM_WINDOW_H