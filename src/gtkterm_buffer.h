#ifndef GTMTERM_BUFFER_H
#define GTKTERM_BUFFER_H

G_BEGIN_DECLS

typedef struct _GtkTermBuffer GtkTermBuffer;

#define GTKTERM_TYPE_BUFFER gtkterm_buffer_get_type ()
G_DECLARE_FINAL_TYPE (GtkTermBuffer, gtkterm_buffer, GTKTERM, BUFFER, GObject)

GtkTermBuffer *gtkterm_buffer_new (void);

G_END_DECLS

#endif // GTKTERM_BUFFER_H