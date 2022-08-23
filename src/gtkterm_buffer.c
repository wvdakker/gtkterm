#include <gtk/gtk.h>
#include <glib.h>
#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <config.h>
#include <glib/gi18n.h>

#include "gtkterm_buffer.h"

typedef struct {


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
 * @brief Initializing the buffer class
 * 
 * Setting the properties and callback functions
 * 
 * @param class The buffer port class
 * 
 */
static void gtkterm_buffer_class_init (GtkTermBufferClass *class) {

  	// GObjectClass *object_class = G_OBJECT_CLASS (class);
    //object_class->set_property = gtkterm_buffer_set_property;     

    //g_object_class_install_properties (object_class, N_PROPS, gtkterm_buffer_properties);
}

/**
 * @brief Initialize the buffe
 * 
 * @param self The bufferwe are initializing.
 * 
 */
static void gtkterm_buffer_init (GtkTermBuffer *self) {
    GtkTermBufferPrivate *priv = gtkterm_buffer_get_instance_private (self);

}