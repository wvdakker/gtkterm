#ifndef GTMTERM_BUFFER_H
#define GTKTERM_BUFFER_H

G_BEGIN_DECLS

#define GTKTERM_TYPE_BUFFER gtkterm_buffer_get_type ()
G_DECLARE_FINAL_TYPE (GtkTermBuffer, gtkterm_buffer, GTKTERM, BUFFER, GObject)
typedef struct _GtkTermBuffer GtkTermBuffer;

GtkTermBuffer *gtkterm_buffer_new (GtkTermSerialPort *, term_config_t *);

G_END_DECLS

int gtkterm_buffer_append_text (GtkTermBuffer *, char *, unsigned int);

#endif // GTKTERM_BUFFER_H