#include <gtk/gtk.h>
#include <glib.h>
#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <config.h>
#include <glib/gi18n.h>

#include "gtkterm_defaults.h"
#include "gtkterm_buffer.h"

typedef struct {
    char *buffer;

} GtkTermBufferPrivate;

struct _GtkTermBuffer {
    GObject parent_instance;
};

struct _GtkTermBufferClass {
    GObjectClass parent_class;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkTermBuffer, gtkterm_buffer, G_TYPE_OBJECT)

/**
 * @brief Create a new buffer object
 * 
 * @return The buffer object.
 * 
 */
GtkTermBuffer *gtkterm_buffer_new (void) {

    return g_object_new (GTKTERM_TYPE_BUFFER, NULL);
}

/**
 * @brief Finalizing the buffer class
 * 
 * Clears the pointer of the buffer.
 * 
 * @param object The object which is finialized
 * 
 */
static void gtkterm_buffer_finalize (GObject *object) {
    GtkTermBuffer *self = GTKTERM_BUFFER (object);
    GtkTermBufferPrivate *priv = gtkterm_buffer_get_instance_private (self);
    GObjectClass *object_class = G_OBJECT_CLASS (gtkterm_buffer_parent_class);

    g_clear_pointer (&priv->buffer, g_free);

    object_class->finalize (object);    
}

/**
 * @brief Initializing the buffer class
 * 
 * Setting the properties and callback functions
 * 
 * @param class The buffer port class
 * 
 */
static void gtkterm_buffer_class_init (GtkTermBufferClass *class) {
    GObjectClass *object_class = G_OBJECT_CLASS (class);
    //object_class->set_property = gtkterm_buffer_set_property;
    object_class->finalize = gtkterm_buffer_finalize;

    //g_object_class_install_properties (object_class, N_PROPS, gtkterm_buffer_properties);
}

/**
 * @brief Initialize the buffer with size BUFFER_SIZE.
 * 
 * @param self The buffer we are initializing.
 * 
 */
static void gtkterm_buffer_init (GtkTermBuffer *self) {
    GtkTermBufferPrivate *priv = gtkterm_buffer_get_instance_private (self);

    priv->buffer = g_malloc0 (BUFFER_SIZE);
}