#ifndef GTMTERM_BUFFER_H
#define GTKTERM_BUFFER_H

/**
 * @brief  Enum buffer_error id.
 * 
 * Many of the gtk_buffer functions return
 * an error id.
 */
typedef enum {
	GTKTERM_BUFFER_SUCCESS,
	GTKTERM_BUFFER_NOT_INITALIZED,
	GTKTERM_BUFFER_OVERFLOW,
	GTKTERM_BUFFER_LAST

} GtkTermBufferState;

G_BEGIN_DECLS

#define GTKTERM_TYPE_BUFFER gtkterm_buffer_get_type ()
G_DECLARE_FINAL_TYPE (GtkTermBuffer, gtkterm_buffer, GTKTERM, BUFFER, GObject)
typedef struct _GtkTermBuffer GtkTermBuffer;

GtkTermBuffer *gtkterm_buffer_new (GtkTermSerialPort *, GtkTermTerminal *, term_config_t *);

G_END_DECLS

GtkTermBufferState gtkterm_buffer_get_status (GtkTermBuffer *);
GError *gtkterm_buffer_get_error (GtkTermBuffer*);

#endif // GTKTERM_BUFFER_H