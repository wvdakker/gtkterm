#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#ifndef GTKTERM_WINDOW_H
#define GTKTERM_WINDOW_H

G_BEGIN_DECLS

#define GTKTERM_TYPE_GTKTERM_WINDOW gtkterm_window_get_type()
typedef struct _GtkTermWindow GtkTermWindow;
G_DECLARE_FINAL_TYPE (GtkTermWindow, gtkterm_window, GTKTERM, WINDOW, GtkApplicationWindow)

G_END_DECLS

void create_window (GApplication *);

#endif // GTKTERM_WINDOW_H