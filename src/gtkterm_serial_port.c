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
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <config.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <glib-unix.h>
#include <gudev/gudev.h>

#include "gtkterm_defaults.h"
#include "gtkterm_serial_port.h"

#ifdef HAVE_LINUX_SERIAL_H
#include <linux/serial.h>
#endif

const char GtkTermSerialPortStateString [][DEFAULT_STRING_LEN] = {
	"Connected",
	"Disconnected",
	"Error"
};

typedef struct {
	GUdevClient *udev_client;			/**< The udev client for monitoring the device status 	*/
    port_config_t *port_conf;			/**< The configuration for the serial port				*/
    struct termios port_termios;		/**< Saved port termios configuration for this port		*/

    int port_fd;						/**< The port file descriptor							*/
	GtkTermSerialPortState port_status;	/**< State of the serial port							*/
	GError *port_error;					//*< Last error detected on the port					*/

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
	PROP_PORT_STATUS,
	N_PROPS 
};

static GParamSpec *gtkterm_serial_port_properties[N_PROPS] = {NULL};

/** Local functions */
static int gtkterm_serial_port_open (GtkTermSerialPort *);
static int gtkterm_serial_port_close (GtkTermSerialPort *);
int gtkterm_serial_port_lock (GtkTermSerialPort *, GError **);
int gtkterm_serial_port_unlock (GtkTermSerialPort *);
void gtkterm_serial_port_set_status (GtkTermSerialPort *, GtkTermSerialPortState, GError *);

static bool gtkterm_serial_port_handle_usr1 (gpointer);
static bool gtkterm_serial_port_handle_usr2 (gpointer);

#ifdef HAVE_LINUX_SERIAL_H
int gtkterm_serial_port_set_custom_speed(int, int);
#endif

/**
 * @brief Create a new serial port object.
 * 
 * This also binds the parameter to the properties of the serial port.
 * 
 * @param port_conf The section for the configuration in this terminal
 * 
 * @return The serial_port object.
 * 
 */
GtkTermSerialPort *gtkterm_serial_port_new (port_config_t *port_conf) {

    return g_object_new (GTKTERM_TYPE_SERIAL_PORT, "port_conf", port_conf, NULL);
}

/**
 * @brief Opens or closes the serial port.
 * 
 * @param self The serial port structure.
 * 
 * @param status The new status for this port
 */
static inline void gtkterm_serial_port_set(GtkTermSerialPort *self, GtkTermSerialPortStatus status) {
	if (status == GTKTERM_SERIAL_PORT_OPEN)
		gtkterm_serial_port_open (self);
	else
		gtkterm_serial_port_close (self);	
}

/**
 * @brief Based on the action return from uevent we handle to open of close the port
 * 
 * Note: This is an inline function. Without inline it wont work!.
 * 
 * @param self Pointer to the serial port.
 * 
 * @param action The action we are performing with this device.
 */
static inline void gtkterm_serial_port_handle(GtkTermSerialPort *self, const char *action) {

	if (g_strcmp0(action, "remove") == 0)
		gtkterm_serial_port_set (self, GTKTERM_SERIAL_PORT_CLOSE);
	else if (g_strcmp0 (action, "add") == 0)
		gtkterm_serial_port_set (self, GTKTERM_SERIAL_PORT_OPEN);
}

/**
 * @brief Callback for the uevent signal
 * 
 * @param client The udev-client
 * 
 * @param action The action it detects
 * 
 * @param device The device.
 * 
 * @param user_data The GtkTermSerialPort structure.
 */
void gtkterm_serial_port_event_udev(GUdevClient *client, const char *action, GUdevDevice *device, gpointer user_data) {
	GtkTermSerialPort *self = GTKTERM_SERIAL_PORT(user_data);
    GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private (self);	

	if (!device || !action)
		return;


	if (!g_udev_device_get_device_file(device))
		return;

	/** Check if the uevent device file matches the port of this configuration */
	if (g_strcmp0 (g_udev_device_get_device_file(device), priv->port_conf->port) == 0)
		gtkterm_serial_port_handle (self, action);
}

extern void gtkterm_serial_port_device_monitor (GtkTermSerialPort *self) {
    GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private (self);	
	const char *const subsystems[] = {NULL, NULL};

	/** Create the udev client */
	priv->udev_client = g_udev_client_new(subsystems);

	/** Get the initial status of the device */
	if (g_udev_client_query_by_device_file(priv->udev_client, priv->port_conf->port) == NULL) {
		gtkterm_serial_port_set (self, GTKTERM_SERIAL_PORT_CLOSE);
	} else {
		gtkterm_serial_port_set (self, GTKTERM_SERIAL_PORT_OPEN);
	}

	/** 
	 * Monitor udev devices
	 * We add self as parameter so we can access the port configuration if needed.
	 */
	g_signal_connect(G_OBJECT(priv->udev_client), "uevent", G_CALLBACK(gtkterm_serial_port_event_udev), self);
}

static bool gtkterm_serial_port_config (GtkTermSerialPort *self,
                                    struct termios *termios_p,
                                    GError **error) {
    GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private (self);

    switch (priv->port_conf->baudrate) {
		case 300:
			termios_p->c_cflag = B300;
			break;
		case 600:
			termios_p->c_cflag = B600;
			break;
		case 1200:
			termios_p->c_cflag = B1200;
			break;
		case 2400:
			termios_p->c_cflag = B2400;
			break;
		case 4800:
			termios_p->c_cflag = B4800;
			break;
		case 9600:
			termios_p->c_cflag = B9600;
			break;
		case 19200:
			termios_p->c_cflag = B19200;
			break;
		case 38400:
			termios_p->c_cflag = B38400;
			break;
		case 57600:
			termios_p->c_cflag = B57600;
			break;
		case 115200:
			termios_p->c_cflag = B115200;
			break;

		default:
#ifdef HAVE_LINUX_SERIAL_H
			gtkterm_serial_port_set_custom_speed (priv->port_fd, priv->port_conf->baudrate);
			termios_p->c_cflag |= B38400;
#else
			g_propagate_error (
				error,
				g_error_new_literal (G_IO_ERROR,
									G_IO_ERROR_FAILED,
									_ ("Arbitrary baud rates not supported")));
			return FALSE;
#endif
    }

    switch (priv->port_conf->bits) {
		case 5:
			termios_p->c_cflag |= CS5;
			break;
		case 6:
			termios_p->c_cflag |= CS6;
			break;
		case 7:
			termios_p->c_cflag |= CS7;
			break;
		case 8:
			termios_p->c_cflag |= CS8;
			break;
		default:
			g_assert_not_reached ();
    }

    switch (priv->port_conf->parity) {
		case GTKTERM_SERIAL_PORT_PARITY_NONE:
			// Nothing to set
			break;			
		case GTKTERM_SERIAL_PORT_PARITY_ODD:
			termios_p->c_cflag |= PARODD | PARENB;
			break;			
		case GTKTERM_SERIAL_PORT_PARITY_EVEN:
			termios_p->c_cflag |= PARENB;
			break;
		default:
			break;
    }

    if (priv->port_conf->stopbits == 2)
        termios_p->c_cflag |= CSTOPB;

	/** set control / enable receiver */
    termios_p->c_cflag |= CREAD;
	/** ignore break and framing errors */
    termios_p->c_iflag = IGNPAR | IGNBRK;

    switch (priv->port_conf->flow_control) {
		case GTKTERM_SERIAL_PORT_FLOWCONTROL_XON_XOFF:
			termios_p->c_iflag |= IXON | IXOFF;
			break;
		case GTKTERM_SERIAL_PORT_FLOWCONTROL_RTS_CTS:
			termios_p->c_cflag |= CRTSCTS;
			break;
		case GTKTERM_SERIAL_PORT_FLOWCONTROL_RS485_HD:
		default:
			termios_p->c_cflag |= CLOCAL;
			break;
    }

	/** clear output and local mode */
    termios_p->c_oflag = 0;
    termios_p->c_lflag = 0;

    termios_p->c_cc[VTIME] = 0;	 /**< Timeout in deciseconds for noncanonical read 	*/
    termios_p->c_cc[VMIN] = 1;	 /**< Minimal characters for noncanonical read 		*/

    return TRUE;
}

/**
 * @brief Connects to the serial port.
 *
 * The settings for this port will be set.
 *
 * @param self The configuration class.
 *
 * @return The result of the operation.
 */
static int gtkterm_serial_port_open (GtkTermSerialPort *self) {
	GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private(self);	
    struct termios termios_p;
    GError *error = NULL;

 //   priv->cancellable = g_cancellable_new ();
    priv->port_fd = open (priv->port_conf->port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (priv->port_fd == -1) {
        error = g_error_new (G_IO_ERROR,
                             g_io_error_from_errno (errno),
                             _ ("Cannot open %s: %s"),
                             priv->port_conf->port,
                             g_strerror (errno));

        gtkterm_serial_port_set_status (self, GTKTERM_SERIAL_PORT_ERROR, error);
        return false;
    }

    if (gtkterm_serial_port_lock (self, &error)) {
        gtkterm_serial_port_close (self);
        gtkterm_serial_port_set_status (self, GTKTERM_SERIAL_PORT_ERROR, error);

        return false;
    }

	/** get termios for the file descriptor */
    tcgetattr (priv->port_fd, &termios_p);
	/** And back it up */
    memcpy (&(priv->port_termios), &termios_p, sizeof (struct termios));

    if (!gtkterm_serial_port_config (self, &termios_p, &error)) {
		gtkterm_serial_port_close (self);
        gtkterm_serial_port_set_status (self, GTKTERM_SERIAL_PORT_ERROR, error);
		
		return false;
    }

	/** Set the created termios to the file descriptor */
    tcsetattr (priv->port_fd, TCSANOW, &termios_p);

	/** Flush the in- output data which are not written/read */
    tcflush (priv->port_fd, TCOFLUSH);
    tcflush (priv->port_fd, TCIFLUSH);

//    priv->input_stream = g_unix_input_stream_new (priv->serial_port_fd, FALSE);
//    priv->output_stream = g_unix_output_stream_new (priv->serial_port_fd, FALSE);

 //   g_input_stream_read_bytes_async (priv->input_stream,
 //                                    RECEIVE_BUFFER_SIZE,
 //                                    G_PRIORITY_DEFAULT,
 //                                    priv->cancellable,
 //                                    gt_serial_port_on_data_ready,
 //                                    self);

    gtkterm_serial_port_set_status (self, GTKTERM_SERIAL_PORT_CONNECTED, NULL);

//    priv->status_timeout =
//        g_timeout_add (GT_SERIAL_PORT_CONTROL_POLL_DELAY,
//                       gt_serial_port_on_control_signals_read,
//                       self);

    return true;
}

/**
 * @brief Closes the serial port.
 * 
 * It check is there is an open port and if yes it closes the port.
 * 
 * @param self The Serial Port structure.
 * 
 * @return int Result of the operation
 */
static int gtkterm_serial_port_close (GtkTermSerialPort *self) {
	GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private(self);	
	
	if(priv->port_fd != -1) {

		tcsetattr(priv->port_fd , TCSANOW, &priv->port_termios);

		/** Flush input and output data */
		tcflush(priv->port_fd , TCOFLUSH);
		tcflush(priv->port_fd, TCIFLUSH);

		gtkterm_serial_port_unlock (self);

		close(priv->port_fd);
		priv->port_fd = -1;
	}

    gtkterm_serial_port_set_status (self, GTKTERM_SERIAL_PORT_DISCONNECTED, NULL);

	return true;
}

/**
 * @brief Convert port config to a string.
 * 
 * This is used for setting the configuration in the statusbar and window title.
 * 
 * @param self The SerialPort structure
 * 
 * @return The port configuration as string.
 */
char* gtkterm_serial_port_get_string (GtkTermSerialPort *self)
{
	char* msg;
	char parity;
	GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private(self);	

	if(priv->port_fd == -1)	{
		msg = g_strdup(_("No open port"));
	} else {
		// 0: none, 1: odd, 2: even
		switch(priv->port_conf->parity)
		{
			case GTKTERM_SERIAL_PORT_PARITY_NONE:
				parity = 'N';
				break;
			case GTKTERM_SERIAL_PORT_PARITY_ODD:
				parity = 'O';
				break;
			case GTKTERM_SERIAL_PORT_PARITY_EVEN:
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

/**
 * @brief  Connect callback functions to signals.
 *
 * The callback functions performs the operation on the keyfile or configuration.
 *
 * @param object The configuration class object.
 *
 */
static void gtkterm_serial_port_class_constructed(GObject *object) {
	GtkTermSerialPort *self = GTKTERM_SERIAL_PORT(object);

	gtkterm_serial_port_device_monitor (self);

	/** Add the user signals */
	g_unix_signal_add(SIGUSR1, (GSourceFunc) gtkterm_serial_port_handle_usr1, self);
	g_unix_signal_add(SIGUSR2, (GSourceFunc) gtkterm_serial_port_handle_usr2, self);	
}

/**
 * @brief get the property of the GtkTermSerialPort structure
 * 
 * This is used to update the properties. For now it is uses
 * to update the notify.
 * 
 * @param object The object.
 * 
 * @param prop_id The id of the property to get.
 * 
 * @param value The value for the property
 * 
 * @param pspec Metadata for property setting.
 * 
 */
static void gtkterm_serial_port_get_property (GObject *object,
                             unsigned int prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    GtkTermSerialPort *self = GTKTERM_SERIAL_PORT(object);
    GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private (self);

    switch (prop_id) {
    	case PROP_PORT_STATUS:
        	g_value_set_int (value, priv->port_status);
        	break;		

    	default:
        	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

/**
 * @brief Set the property of the GtkTermSerialPort structure
 * 
 * This is used to initialize the variables when creating a new serial_port
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

/**
 * @brief Remote all pointers when removing the object.
 * 
 * @param object The pointer to the serial port object.
 */
static void gtkterm_serial_port_finalize (GObject *object) {
    GtkTermSerialPort *self = GTKTERM_SERIAL_PORT(object);
    GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private (self);

    g_clear_error (&priv->port_error);

    GObjectClass *object_class = G_OBJECT_CLASS (gtkterm_serial_port_parent_class);
    object_class->finalize (object);
}

/**
 * @brief Initializing the serial_port class
 * 
 * Setting the properties and callback functions
 * 
 * @param class The serial_portclass
 * 
 */
static void gtkterm_serial_port_class_init (GtkTermSerialPortClass *class) {

  	GObjectClass *object_class = G_OBJECT_CLASS (class);
    object_class->set_property = gtkterm_serial_port_set_property;
    object_class->get_property = gtkterm_serial_port_get_property;	
	object_class->constructed = gtkterm_serial_port_class_constructed;
	object_class->finalize = gtkterm_serial_port_finalize;	

	/**
	 *  Parameters to hand over at creation of the object
	 */
  	gtkterm_serial_port_properties[PROP_PORT_CONFIG] = g_param_spec_pointer (
        "port_conf",
        "port_conf",
        "port_conf",
        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  	gtkterm_serial_port_properties[PROP_PORT_STATUS] = g_param_spec_int (
        "port-status",
        "port-status",
        "port-status",
		0,
		G_MAXINT,
		0,
        G_PARAM_READABLE| G_PARAM_STATIC_STRINGS);    		

    g_object_class_install_properties (object_class, N_PROPS, gtkterm_serial_port_properties);
}

/**
 * @brief Initialize the serial with the config parameters
 * 
 * @param self The port we are initializing.
 * 
 */
static void gtkterm_serial_port_init (GtkTermSerialPort *self) {
    GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private (self);

	//! Not yet connected
	priv->port_fd = -1;
}

int gtkterm_serial_port_unlock (GtkTermSerialPort *self) {
    GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private (self);
	int rc = 0;

	if (!priv->port_conf->disable_port_lock)
		rc = flock(priv->port_fd, LOCK_UN);

	return rc;
}

int gtkterm_serial_port_lock (GtkTermSerialPort *self, GError **error) {
    GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private (self);
	int rc = 0;

	if (!priv->port_conf->disable_port_lock) {
		rc = flock(priv->port_fd, LOCK_EX | LOCK_NB);

		if (rc) {
			gtkterm_serial_port_close (self);

			g_propagate_error (error, 
								g_error_new (G_IO_ERROR, g_io_error_from_errno (errno),
								_("Can't get lock on port %s: %s"), priv->port_conf->port, g_strerror (errno))
								);
		}
	}

	return rc;	
}

void gtkterm_serial_port_set_status (GtkTermSerialPort *self, GtkTermSerialPortState new_status, GError *error) {
    GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private (self);

	priv->port_status = new_status;

	/** If there is a previous error, clear it */
	if (priv->port_error != NULL)
		g_error_free (priv->port_error);
	
	priv->port_error = error;

	/** Notify that we have a change on the port */
	g_object_notify(G_OBJECT(self), "port-status");		
}

GtkTermSerialPortState gtkterm_serial_port_get_status (GtkTermSerialPort *self) {
    GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private (self);

	return (priv->port_status);
}

GError *gtkterm_serial_port_get_error (GtkTermSerialPort *self) {
    GtkTermSerialPortPrivate *priv = gtkterm_serial_port_get_instance_private (self);

	return (priv->port_error);
}

/**
 * @brief Handles USR1 signal. It opens the port.
 * 
 * @param user_data The serial port structure. Last param when installing the signal
 * 
 * @return Continue the signal operation.
 */
static bool gtkterm_serial_port_handle_usr1(gpointer user_data) {
	GtkTermSerialPort *self = GTKTERM_SERIAL_PORT(user_data);

	gtkterm_serial_port_open (self);

	return G_SOURCE_CONTINUE;
}

/**
 * @brief Handles USR2 signal. It closes the port.
 * 
 * @param user_data The serial port structure. Last param when installing the signal
 * 
 * @return Continue the signal operation.
 */
static bool gtkterm_serial_port_handle_usr2(gpointer user_data) {
	GtkTermSerialPort *self = GTKTERM_SERIAL_PORT(user_data);

	gtkterm_serial_port_close (self);
	return G_SOURCE_CONTINUE;
}

#ifdef HAVE_LINUX_SERIAL_H
/**
 * @brief Set the custom baudrate for a port
 * 
 * This can only be done on Linux systems.
 * 
 * @param port_fd File descriptor of the serial port.
 * 
 * @param baudrate The baudrate we try so set
 * 
 * @return int The result of the change.
 */
int gtkterm_serial_port_set_custom_speed(int port_fd, int baudrate) {
	struct serial_struct serial_conf;

	ioctl(port_fd, TIOCGSERIAL, &serial_conf);
	serial_conf.custom_divisor = serial_conf.baud_base / baudrate;
	if(!(serial_conf.custom_divisor))
		serial_conf.custom_divisor = 1;

	serial_conf.flags &= ~ASYNC_SPD_MASK;
	serial_conf.flags |= ASYNC_SPD_CUST;

	ioctl(port_fd, TIOCSSERIAL, &serial_conf);

	return 0;
}
#endif
