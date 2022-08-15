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
 //   GOutputStream *output_stream;
 //   GInputStream *input_stream;
    port_config_t port_conf;
    struct termios termios_save;
    int serial_port_fd;
 //   char lockfile[256];

 //   GtSerialPortState state;
 //   GError *last_error;
 //   int control_flags;
 //   unsigned int status_timeout;
//    GtkTermBuffer *buffer;
//    GCancellable *cancellable;
} GtkTermSerialPortPrivate;

struct _GtkTermSerialPort {
    GObject parent_instance;
};

struct _GtkTermSerialPortClass {
    GObjectClass parent_class;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkTermSerialPort, gtkterm_serial_port, G_TYPE_OBJECT)

static void gtkterm_serial_port_class_init (GtkTermSerialPortClass *classf) {

}

static void gtkterm_serial_port_init (GtkTermSerialPort *self) {

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
		switch(priv->port_conf.parity)
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
		                      priv->port_conf.port,
		                      priv->port_conf.baudrate,
		                      priv->port_conf.bits,
		                      parity,
		                      priv->port_conf.stopbits
		                     );
	}

	return msg;
}
