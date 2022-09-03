/************************************************************************/
/* gtkterm_buffer.c                                                   	*/
/* ----------------                                                    	*/
/*           GTKTerm Software                                          	*/
/*                      (c) Julien Schmitt                             	*/
/*                                                                     	*/
/* ------------------------------------------------------------------- 	*/
/*                                                                     	*/
/*   Purpose                                                           	*/
/*      Main program file                                              	*/
/*                                                                     	*/
/*   ChangeLog                                                         	*/
/*      - 2.0 	 : Ported to GTK4                                     	*/
/*      - 0.99.7 : removed (send)auto crlf stuff - (use macros instead) */
/*      - 0.99.5 : Corrected segfault in case of buffer overlap         */
/*      - 0.99.2 : Internationalization                                 */
/*      - 0.98.4 : file creation by Julien                              */
/*                                                                     	*/
/* This GtkTerm is free software: you can redistribute it and/or modify	*/ 
/* it under the terms of the GNU  General Public License as published  	*/
/* by the Free Software Foundation, either version 3 of the License,   	*/
/* or (at your option) any later version.							   	*/
/*																	   	*/
/* GtkTerm is distributed in the hope that it will be useful, but	   	*/
/* WITHOUT ANY WARRANTY; without even the implied warranty of 		   	*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 			   	*/
/* See the GNU General Public License for more details.					*/
/*																		*/
/* You should have received a copy of the GNU General Public License 	*/
/* along with GtkTerm If not, see <https://www.gnu.org/licenses/>. 		*/
/*                                                                     	*/
/************************************************************************/

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

#define TIMESTAMP_SIZE 50

typedef struct {
    char *buffer;								/**< The actual buffer										*/
	size_t tail;								/**< The tail of the buffer									*/
	term_config_t *term_conf;

	bool lf_received;							/**< Reminder if we have a LF received						*/
	bool cr_received;							/**< Reminder if we have a CR received						*/
	bool need_to_write_timestamp;				/**< Reminder we need to write the timestamp (after a CR)	*/

	GError *config_error;						/**< Error of the last file operation						*/
	GtkTermConfigurationState config_status; 	/**< Status when operating the buffer						*/	

	GtkTermSerialPort *serial_port;				/**< For connecting to the serial-data-received signals 	*/
	GtkTermTerminal *terminal;					/**< For connecting to the vte-data-received signals 		*/
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
	PROP_TERMINAL,		
	PROP_TERM_CONF,	
	N_PROPS 
};

static GParamSpec *gtkterm_buffer_properties[N_PROPS] = {NULL};

static GtkTermBufferState gtkterm_buffer_add_data (GObject *, gpointer, gpointer);
GtkTermBufferState gtkterm_buffer_set_status(GtkTermBuffer *, GtkTermBufferState, GError *);
unsigned int insert_timestamp (char *);
void gtkterm_buffer_repage (GtkTermBuffer *);

/**
 * @brief Create a new buffer object
 * 
 * @return The buffer object.
 * 
 */
GtkTermBuffer *gtkterm_buffer_new (GtkTermSerialPort * serial_port, GtkTermTerminal *terminal, term_config_t *term_conf) {

    return g_object_new (GTKTERM_TYPE_BUFFER, "serial_port", serial_port, "terminal", terminal, "term_conf", term_conf, NULL);
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
 * @brief Constructs the buffer.
 * 
 * Setup signals etc.
 * 
 * @param object The buffer object we are constructing.
 * 
 */
static void gtkterm_buffer_constructed (GObject *object) {
    GtkTermBuffer *self = GTKTERM_BUFFER(object);
    GtkTermBufferPrivate *priv = gtkterm_buffer_get_instance_private (self);   

	/** Connect to data received signals from vte and serial */
   	g_signal_connect (priv->serial_port, "serial-data-received", G_CALLBACK(gtkterm_buffer_add_data), self);
  	g_signal_connect (priv->terminal, "vte-data-received", G_CALLBACK(gtkterm_buffer_add_data), self);

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
	g_free(priv->terminal);
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

    	case PROP_TERMINAL:
        	priv->terminal = g_value_dup_object(value);
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

  	gtkterm_buffer_properties[PROP_TERMINAL] = g_param_spec_object (
        "terminal",
        "terminal",
        "terminal",
        GTKTERM_TYPE_TERMINAL,
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
	priv->tail = 0;

	priv->lf_received = false;
	priv->cr_received = false;
	priv->need_to_write_timestamp = false;
}

/**
 * @brief New data is available to add to the buffer.
 * 
 * The buffer will signal the terminal new data is available.
 * 
 * @param object Not used.
 * 
 * @param data The new byte-string of data for the buffer.
 * 
 * @param user_data The buffer.
 * 
 */
static GtkTermBufferState gtkterm_buffer_add_data (GObject *object, gpointer data, gpointer user_data) {
    GtkTermBuffer *self = GTKTERM_BUFFER(user_data);
    GtkTermBufferPrivate *priv = gtkterm_buffer_get_instance_private (self);
	size_t str_size;
	size_t old_tail;
    const char *string = g_bytes_get_data ((GBytes *)data, &str_size);

	if(priv->buffer == NULL) {
		GError *error;
		error = g_error_new (g_quark_from_static_string ("GTKTERM_BUFFER"),
                             GTKTERM_BUFFER_NOT_INITALIZED,
                             _("Buffer not initialized")
                             );

		return gtkterm_buffer_set_status (self, GTKTERM_BUFFER_NOT_INITALIZED, error);
	}

	/**
     * (When incoming size is larger than buffer, then just print the
	 * last BUFFER_SIZE characters and ignore all other at begin of buffer)
	 * In theory the str_size (8k) cannot be larger then the buffer size (128k)
	 * 
	 * \todo Make buffer also work with greater datastrings to make tbe above work.
     */
	if (str_size > BUFFER_SIZE) {
		string += (str_size - BUFFER_SIZE);
		str_size = BUFFER_SIZE;

		GError *error;
		error = g_error_new (g_quark_from_static_string ("GTKTERM_BUFFER"),
                             GTKTERM_BUFFER_OVERFLOW,
                             _("Buffer overflow")
                             );

		return gtkterm_buffer_set_status (self, GTKTERM_BUFFER_OVERFLOW, error);
	}

	/**
     * When incoming size is larger than free space in the buffer
	 * then repage the buffer which will move the head.
	 * We use 2x the str_size to be sure.
     */
	if ((2 * str_size) > (BUFFER_SIZE - priv->tail))
		gtkterm_buffer_repage (self);

	/** Indicates from where to send data to the terminal */
	old_tail = priv->tail;

	/** If the auto CR or LF mode on, read the buffer to add LF before CR */
	if (priv->term_conf->auto_lf || priv->term_conf->auto_cr || priv->term_conf->timestamp) {

		for (int i = 0; i < str_size; i++) {

            if (priv->term_conf->auto_cr) {

			 	/** If the previous character was a CR too, insert a newline */
			 	if (string[i] == '\r' && priv->cr_received) {
			 		
					*(priv->buffer + priv->tail) = '\n';
			 		priv->tail++;
			 		priv->need_to_write_timestamp = 1;

					priv->cr_received = 1;
			 	}
			} 

			if (priv->term_conf->auto_lf) {
			 		if (string[i] == '\n') {

			 			/* If we get a newline without a CR first, insert a CR */
			 			if (!priv->cr_received) {
			 				*(priv->buffer + priv->tail) = '\r';
			 				priv->tail++;
			 			}
			 		} else {
			 			/** If we receive a normal char, and the previous one was a CR insert a newline */
			 			if (priv->cr_received) {
			 				*(priv->buffer + priv->tail) = '\n';
			 				priv->tail++;
			 				priv->need_to_write_timestamp = 1;
			 			}
					}

			 	priv->cr_received = 0;
			}

			/** If we have timestamps configured and it is time to print one, print it */
		    if (priv->term_conf->timestamp && priv->need_to_write_timestamp) {

			 	priv->tail += insert_timestamp (priv->buffer + priv->tail);
				priv->need_to_write_timestamp = 0;
			}

			if (string[i] == '\n')
			 	priv->need_to_write_timestamp = 1; 					/**< remember until we have a new character to print	*/

			*(priv->buffer + priv->tail) = string[i];				/**< Copy the string character to the buffer			*/
			priv->tail++; 											/**< Increment for each stored character				*/

			/** If we are growing out of the buffer, then repage */
			if ((priv->tail + 2 * TIMESTAMP_SIZE) >= BUFFER_SIZE)
				gtkterm_buffer_repage (self);
		}
	} else {

		/** We dont need any timestamps or LF/CR so just copy the string into the buffer */
		memcpy ((priv->buffer + priv->tail), string, str_size);
		priv->tail += str_size;	
	}

//	g_printf ("T %ld S %ld O %ld\n", priv->tail, str_size, old_tail);

    g_bytes_unref (data);	

	/** Send new data to the terminal */
	g_signal_emit (self, 
					gtkterm_signals[SIGNAL_GTKTERM_BUFFER_UPDATED], 0, 
					(priv->buffer + old_tail), 
					priv->tail - old_tail);

	return GTKTERM_BUFFER_SUCCESS;				
}

/**
 * @brief Add a timestamp to the buffer.
 * 
 * Assumes that buffer always has space for timestamp (TIMESTAMP_SIZE)
 * 
 * @param buffer Points to location where timestamp will be inserted.
 * 
 * @return unsigned int Length of the timestamp added.
 * 
 */
unsigned int insert_timestamp (char *buffer) {
  	unsigned int timestamp_length = 0;
	struct timespec ts;
	int d,h,m,s,x;

	timespec_get(&ts, TIME_UTC);
	d = (ts.tv_sec / (3600 * 24));
	h = (ts.tv_sec / 3600) % 24;
	m = (ts.tv_sec / 60 ) % 60;
	s = ts.tv_sec % 60;
	x = ts.tv_nsec / 1000000;

	g_snprintf(buffer, TIMESTAMP_SIZE - 1, "[%d.%02uh.%02um.%02us.%03u] ", d, h, m, s, x );

	timestamp_length = strlen(buffer);

  	return timestamp_length;
}

/**
 * @brief  Repage the buffer to make room for new data
 * 
 * When repaging thebuffer is moved 2 pages up.
 * The head is always placed after a CR to make sure we start with a new string.
 */
void gtkterm_buffer_repage (GtkTermBuffer *self) {
    GtkTermBufferPrivate *priv = gtkterm_buffer_get_instance_private (self);

//	g_printf ("Repaging ...\n");

	memmove (priv->buffer + 16 * 1024, priv->buffer, BUFFER_SIZE - 16 * 1024);
	priv->tail = BUFFER_SIZE - 16 * 1024;

	while (*(priv->buffer + priv->tail) != '\n')
		priv->tail++;

//	g_printf ("Repaging ready %ld...\n", priv->tail);		
}

/**
 * @brief  Sets the status and error of the last operation.
 *
 * @param self The configuration for which the get the status for.
 *
 * @param status The status to be set.
 * 
 * @param error The error message (can be NULL)
 *
 * @return  The latest status.
 *
 */
GtkTermBufferState gtkterm_buffer_set_status (GtkTermBuffer *self, GtkTermBufferState status, GError *error) {
    GtkTermBufferPrivate *priv = gtkterm_buffer_get_instance_private (self);

	priv->config_status = status;

	/** If there is a previous error, clear it */
	if (priv->config_error != NULL)
		g_error_free (priv->config_error);
	
	priv->config_error = error;	

	return status;
}

/**
 * @brief  Return the latest status condiation for the buffer operation.
 *
 * @param self The configuration for which the get the status for.
 *
 * @return  The latest status.
 *
 */
GtkTermBufferState gtkterm_buffer_get_status (GtkTermBuffer*self) {
    GtkTermBufferPrivate *priv = gtkterm_buffer_get_instance_private (self);

	return (priv->config_status);
}

/**
 * @brief  Return the latest error for the buffer operation.
 *
 * @param self The configuration for which the get the status for.
 *
 * @return  The latest error.
 *
 */
GError *gtkterm_buffer_get_error (GtkTermBuffer *self) {
    GtkTermBufferPrivate *priv = gtkterm_buffer_get_instance_private (self);

	return (priv->config_error);
}