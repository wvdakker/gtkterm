/***********************************************************************/
/* serial.c                                                             */
/* -------                                                             */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Serial port access functions                                   */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.7 : Removed auto crlf stuff - (use macros instead)      */
/*      - 0.99.5 : changed all calls to strerror() by strerror_utf8()  */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.98.6 : new sendbreak() function                            */
/*      - 0.98.1 : lockfile implementation (based on minicom)          */
/*      - 0.98 : removed IOChannel                                     */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#include <glib.h>
#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <config.h>
#include <glib/gi18n.h>

#include "serial.h"
#include "defaults.h"

#ifdef HAVE_LINUX_SERIAL_H
#include <linux/serial.h>
#endif

typedef struct {
    port_config_t *port_conf;
    struct termios termios_save;
    int serial_port_fd;

} GtkTermSerialPortPrivate;

struct _GtkTermSerialPort {
    GObject parent_instance;
};

struct _GtkTermSerialPortClass {
    GObjectClass parent_class;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkTermSerialPort, gtkterm_serial_port, G_TYPE_OBJECT)

enum { 
	PROP_0, 
	PROP_PORT_CONFIG,
	N_PROPS 
};

static GParamSpec *gtkterm_serial_port_properties[N_PROPS] = {NULL};


GtkTermSerialPort *gtkterm_serial_port_new (port_config_t *port_conf) {

    return g_object_new (GTKTERM_TYPE_SERIAL_PORT, "port_conf", port_conf, NULL);
}

char* gtkterm_serial_port_get_string (GtkTermSerialPort *self)
{
	char* msg;
	char parity;
	GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private(self);	

	if(priv->serial_port_fd == -1)
	{
		msg = g_strdup(_("No open port"));
	}
	else
	{
		// 0: none, 1: odd, 2: even
		switch(priv->port_conf->parity)
		{
			case 0:
				parity = 'N';
				break;
			case 1:
				parity = 'O';
				break;
			case 2:
				parity = 'E';
				break;
			default:
				parity = 'N';
		}

		/* "GtkTerm: device  baud-bits-parity-stopbits"  */
		msg = g_strdup_printf("%.15s  %ld-%d-%c-%d",
		                      priv->port_conf->port,
		                      priv->port_conf->baudrate,
		                      priv->port_conf->bits,
		                      parity,
		                      priv->port_conf->stopbits
		                     );
	}

	return msg;
}

int gtkterm_serial_port_status (GtkTermSerialPort *self) {
	GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private(self);

	return (priv->serial_port_fd != -1);
}

static void gtkterm_serial_port_set_property (GObject *object,
                             unsigned int prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GtkTermSerialPort *self = GTKTERM_SERIAL_PORT(object);
    GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private (self);

    switch (prop_id) {
    	case PROP_PORT_CONFIG:
        	priv->port_conf = g_value_get_pointer(value);
        	break;		

    	default:
        	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void gtkterm_serial_port_class_init (GtkTermSerialPortClass *class) {

  	GObjectClass *object_class = G_OBJECT_CLASS (class);
    object_class->set_property = gtkterm_serial_port_set_property;

	//! Parameters to hand over at creation of the object
	//! We need the section to load the config from the keyfile.
  	gtkterm_serial_port_properties[PROP_PORT_CONFIG] = g_param_spec_pointer (
        "port_conf",
        "port_conf",
        "port_conf",
        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);        

    g_object_class_install_properties (object_class, N_PROPS, gtkterm_serial_port_properties);
}

static void gtkterm_serial_port_init (GtkTermSerialPort *self) {
    GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private (self);

	//! Not yet connected
	priv->serial_port_fd = -1;
}

