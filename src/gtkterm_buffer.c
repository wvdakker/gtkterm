#include <gtk/gtk.h>
#include <glib.h>
#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <config.h>
#include <glib/gi18n.h>

#include "gtkterm_window.h"
#include "gtkterm_defaults.h"
#include "gtkterm_terminal.h"
#include "gtkterm_serial_port.h"
#include "gtkterm_buffer.h"

typedef struct {
    char *buffer;
	uint32_t head;
	uint32_t tail;
	term_config_t *term_conf;

	bool lf_received;
	bool cr_received;

	GtkTermSerialPort *serial_port;
	GError *error;

} GtkTermBufferPrivate;

struct _GtkTermBuffer {
	
    GObject parent_instance;
};

struct _GtkTermBufferClass {

    GObjectClass parent_class;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkTermBuffer, gtkterm_buffer, G_TYPE_OBJECT)

enum { 
	PROP_0, 
	PROP_SERIAL_PORT,
	PROP_TERM_CONF,	
	N_PROPS 
};

static GParamSpec *gtkterm_buffer_properties[N_PROPS] = {NULL};

static void gtkterm_buffer_serial_data_received (GObject *, gpointer, gpointer);

/**
 * @brief Create a new buffer object
 * 
 * @return The buffer object.
 * 
 */
GtkTermBuffer *gtkterm_buffer_new (GtkTermSerialPort * serial_port, term_config_t *term_conf) {

    return g_object_new (GTKTERM_TYPE_BUFFER, "serial_port", serial_port, NULL);
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

static void gtkterm_buffer_constructed (GObject *object) {
    GtkTermBuffer *self = GTKTERM_BUFFER(object);
    GtkTermBufferPrivate *priv = gtkterm_buffer_get_instance_private (self);   

   	g_signal_connect (G_OBJECT(priv->serial_port), "serial-data-received", G_CALLBACK(gtkterm_buffer_serial_data_received), self);

    G_OBJECT_CLASS (gtkterm_buffer_parent_class)->constructed (object);
}

/**
 * @brief Called when distroying the buffer
 * 
 * This is used to clean up an freeing the variables in the buffer structure.
 * 
 * @param object The object.
 * 
 */
static void gtkterm_buffer_dispose (GObject *object) {
    GtkTermBuffer *self = GTKTERM_BUFFER(object);
    GtkTermBufferPrivate *priv = gtkterm_buffer_get_instance_private (self);	
	
    g_free(priv->term_conf);
    g_free(priv->serial_port);
}

/**
 * @brief Set the property of the GtkTermBuffer structure
 * 
 * This is used to initialize the variables when creating a new buffer
 * 
 * @param object The object.
 * 
 * @param prop_id The id of the property to set.
 * 
 * @param value The value for the property
 * 
 * @param pspec Metadata for property setting.
 * 
 */
static void gtkterm_buffer_set_property (GObject *object,
                             unsigned int prop_id,
                             const GValue *value,
                             GParamSpec *pspec) {

    GtkTermBuffer *self = GTKTERM_BUFFER(object);
    GtkTermBufferPrivate *priv = gtkterm_buffer_get_instance_private (self);

    switch (prop_id) {
    	case PROP_SERIAL_PORT:
        	priv->serial_port = g_value_dup_object (value);
        	break;

    	case PROP_TERM_CONF:
        	priv->term_conf = g_value_get_pointer(value);
        	break;		

    	default:
        	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
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

    object_class->set_property = gtkterm_buffer_set_property;
    object_class->finalize = gtkterm_buffer_finalize;
    object_class->constructed = gtkterm_buffer_constructed;
  	object_class->dispose = gtkterm_buffer_dispose;	

    gtkterm_signals[SIGNAL_GTKTERM_BUFFER_UPDATED] = g_signal_new ("buffer-updated",
                                               GTKTERM_TYPE_BUFFER,
                                               G_SIGNAL_RUN_FIRST,
                                               0,
                                               NULL,
                                               NULL,
                                               NULL,
                                               G_TYPE_NONE,
                                               2,
                                               G_TYPE_POINTER,
                                               G_TYPE_UINT);

	/** 
     * Parameters to hand over at creation of the object
	 * We need the section to load the config from the keyfile.
     */
  	gtkterm_buffer_properties[PROP_SERIAL_PORT] = g_param_spec_object (
        "serial_port",
        "serial_port",
        "serial_port",
        GTKTERM_TYPE_SERIAL_PORT,
       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  	gtkterm_buffer_properties[PROP_TERM_CONF] = g_param_spec_pointer (
        "term_conf",
        "term_conf",
        "term_conf",
        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);		

    g_object_class_install_properties (object_class, N_PROPS, gtkterm_buffer_properties);
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
	priv->head = 0;
	priv->tail = 0;

	priv->lf_received = false;
	priv->cr_received = false;
}

static void gtkterm_buffer_serial_data_received (GObject *object, gpointer data, gpointer user_data) {
    GtkTermBuffer *self = GTKTERM_BUFFER(user_data);
    GtkTermBufferPrivate *priv = gtkterm_buffer_get_instance_private (self);

	size_t str_size;
    const char *string = g_bytes_get_data ((GBytes *)data, &str_size);
//	g_printf ("%s", string);

	/**
     * buffer must still be valid after cr conversion or adding timestamp
	 * only pointer is copied below
     */
//	char out_buffer[(BUFFER_RECEPTION*2) + TIMESTAMP_SIZE];
//	const char *characters;

	/* If the auto CR LF mode on, read the buffer to add \r before \n */
//	if(priv->term_conf->auto_lf || priv->term_conf->auto_cr || priv->term_conf->timestamp)
	{
		int i, out_size = 0;

//		for (i=0; i<size; i++)
		{
            // if(crlf_auto)
			// {
			// 	if (chars[i] == '\r')
			// 	{
			// 		/* If the previous character was a CR too, insert a newline */
			// 		if (cr_received)
			// 		{
			// 			out_buffer[out_size] = '\n';
			// 			out_size++;
			// 			need_to_write_timestamp = 1;
			// 		}
			// 		cr_received = 1;
			// 	}
			// 	else
			// 	{
			// 		if (chars[i] == '\n')
			// 		{
			// 			/* If we get a newline without a CR first, insert a CR */
			// 			if (!cr_received)
			// 			{
			// 				out_buffer[out_size] = '\r';
			// 				out_size++;
			// 			}
			// 		}
			// 		else
			// 		{
			// 			/* If we receive a normal char, and the previous one was a
			// 			   CR insert a newline */
			// 			if (cr_received)
			// 			{
			// 				out_buffer[out_size] = '\n';
			// 				out_size++;
			// 				need_to_write_timestamp = 1;
			// 			}
			// 		}
			// 		cr_received = 0;
			// 	}
			// } //if crlf_auto

			// if(need_to_write_timestamp)
			// {
			// 	out_size += insert_timestamp(&out_buffer[out_size]);
			// 	need_to_write_timestamp = 0;
			// }

			// if(chars[i] == '\n' )
			// {
			// 	need_to_write_timestamp = 1; //remember until we have a new character to print
			// }

			//copy each character to new buffer
//			out_buffer[out_size] = chars[i];
//			out_size++; // increment for each stored character

		} // for

		/**
         * Set "incoming" data pointer to new buffer containing all normal and
		 * converted newline characters
         */
//		chars = out_buffer;
//		size = out_size;
	} // if(crlf_auto || timestamp_on)

	if(priv->buffer == NULL) {
//		i18n_printf(_("ERROR : Buffer is not initialized !\n"));
		return;
	}

	/**
     * When incoming size is larger than buffer, then just print the
	 * last BUFFER_SIZE characters and ignore all other at begin of buffer
     */
//	if(size > BUFFER_SIZE) {
//		characters = chars + (size - BUFFER_SIZE);
//		size = BUFFER_SIZE;
//	} else
	memcpy ((priv->buffer + priv->tail), string, str_size);
	priv->tail += str_size;	

//	if((size + pointer) >= BUFFER_SIZE) {
//		memcpy(current_buffer, characters, BUFFER_SIZE - pointer);
//		chars = characters + BUFFER_SIZE - pointer;
//		pointer = size - (BUFFER_SIZE - pointer);
//		memcpy(buffer, chars, pointer);
//		current_buffer = buffer + pointer;
//		overlapped = 1;
//	} else {
//		memcpy(current_buffer, characters, size);
//		pointer += size;
//		current_buffer += size;
//	}

//	if(write_func != NULL)
//		write_func(characters, size);

    g_bytes_unref (data);	

	g_signal_emit (self, gtkterm_signals[SIGNAL_GTKTERM_BUFFER_UPDATED], 0, string, str_size);
}
