/***********************************************************************/
/* serie.c                                                             */
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

#include <glib.h>
#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "term_config.h"
#include "serial.h"
#include "interface.h"
#include "device_monitor.h"
#include "txqueue.h"
#include "buffer.h"
#include "config_file.h"

#include <config.h>
#include <glib/gi18n.h>



struct termios termios_save;
int serial_port_fd = -1;
static unsigned int serial_port_speed;
static int modem_stat = 0;  /* last known modem signal state; reset on port open */

guint callback_handler_in;
static guint modem_poll_source = 0;
static pthread_t modem_tid = 0;
static int modem_watch_fd = -1;
gboolean callback_activated = FALSE;

static gboolean Lis_port(GIOChannel* src G_GNUC_UNUSED, GIOCondition cond, gpointer data G_GNUC_UNUSED)
{
	gssize bytes_read;
	static gchar c[BUFFER_RECEPTION];
	gssize i;

	if (cond & (G_IO_HUP | G_IO_ERR)) {
		Close_port();
		return G_SOURCE_REMOVE;
	}

	if (!(cond & G_IO_IN))
		return G_SOURCE_CONTINUE;

	bytes_read = BUFFER_RECEPTION;

	while(bytes_read == BUFFER_RECEPTION)
	{
		bytes_read = read(serial_port_fd, c, BUFFER_RECEPTION);
		if(bytes_read > 0)
		{
			put_chars(c, (gsize)bytes_read, term_conf.crlfauto);

			if (config.car != -1)
				{
					for (i = 0; i < bytes_read; i++)
						txqueue_file_got_char(c[i]);
				}
		}
		else if(bytes_read == -1)
		{
			if(errno != EAGAIN)
				perror(config.port);
		}
	}

	return TRUE;
}

/* No-op SIGURG handler used to interrupt TIOCMIWAIT in the modem thread */
static void modem_sigurg_noop(int sig) { (void)sig; }

static gboolean update_modem_idle(gpointer user_data G_GNUC_UNUSED)
{
	int stat;
	if (serial_port_fd != -1 && ioctl(serial_port_fd, TIOCMGET, &stat) == 0)
		show_control_signals(stat);
	return G_SOURCE_REMOVE;
}

/* Poll modem status via lis_sig(); used as a GLib timeout source when
 * TIOCMIWAIT is not supported by the driver. */
static gboolean control_signals_read(gpointer user_data G_GNUC_UNUSED)
{
	int state = lis_sig();
	if (state >= 0)
		show_control_signals(state);
	return TRUE;
}

static gboolean setup_modem_poll(gpointer user_data G_GNUC_UNUSED)
{
	if (modem_poll_source == 0 && serial_port_fd != -1) {
		g_debug("modem status: TIOCMIWAIT not supported by driver, using %dms poll", POLL_DELAY);
		modem_poll_source = g_timeout_add(POLL_DELAY, control_signals_read, NULL);
	}
	return G_SOURCE_REMOVE;
}

static void *modem_thread_func(void *arg)
{
	int fd = (int)(intptr_t)arg;
	int bits = TIOCM_RNG | TIOCM_DSR | TIOCM_CD | TIOCM_CTS | TIOCM_DTR | TIOCM_RTS;
	gboolean first = TRUE;

	while (1) {
		if (ioctl(fd, TIOCMIWAIT, bits) == -1) {
			if (first && errno == EINVAL)
				g_idle_add(setup_modem_poll, NULL);
			break;  /* EINVAL (unsupported), EINTR (SIGURG), or EBADF (fd closed) */
		}
		if (first) {
			g_debug("modem status: TIOCMIWAIT supported, event-driven monitoring active");
			first = FALSE;
		}
		g_idle_add(update_modem_idle, NULL);
	}
	return NULL;
}

int lis_sig(void)
{
	int stat_read;

	if (config.flux == 3)
		Set_signals(1);

	if (serial_port_fd != -1)
	{
		if (ioctl(serial_port_fd, TIOCMGET, &stat_read) == -1)
		{
			/* Ignore EINVAL, as some serial ports
			   genuinely lack these lines */
			/* Thanks to Elie De Brauwer on ubuntu launchpad */
			if (errno != EINVAL)
			{
				g_printerr("%s: %s\n", _("Control signals read lis_sig"), g_strerror(errno));
				Close_port();
			}
			return -2;
		}
		if (stat_read == modem_stat)
			return -1;
		modem_stat = stat_read;
		return modem_stat;
	}
	return -1;
}

gssize Send_chars(const char *string, gsize length)
{
	gssize bytes_written = 0;

	if(serial_port_fd == -1)
		return 0;

	/* Normally it never happens, but it is better not to segfault ;) */
	if(length == 0)
		return 0;

	/* RS485 half-duplex mode ? */
	if( config.flux==3 )
	{
		/* set RTS (start to send) */
		Set_signals( 1 );
		if( config.rs485_rts_time_before_transmit>0 )
			usleep((unsigned int)(config.rs485_rts_time_before_transmit*1000));
	}

	bytes_written = write(serial_port_fd, string, length);
	/* Port is opened O_NDELAY; a full kernel TTY buffer is not a fatal error. */
	if (bytes_written == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		bytes_written = 0;

	/* RS485 half-duplex mode ? */
	if( config.flux==3 )
	{
		/* wait all chars are send */
		tcdrain( serial_port_fd );
		if( config.rs485_rts_time_after_transmit>0 )
			usleep((unsigned int)(config.rs485_rts_time_after_transmit*1000));
		/* reset RTS (end of send, now receiving back) */
		Set_signals( 1 );
	}

	return bytes_written;
}

gboolean Config_port(gboolean show_errors)
{
	struct termios termios_p;
	unsigned int speed_margin;

	Close_port();
	modem_stat = ~0;  /* force UI refresh after open */

	serial_port_fd = open(config.port, O_RDWR | O_NOCTTY | O_NDELAY);

	if(serial_port_fd == -1)
	{
		if (show_errors)
			show_messagef(MSG_ERR, _("Cannot open %s: %s\n"),
			              config.port, g_strerror(errno));
		return FALSE;
	}

	if (!isatty(serial_port_fd))
	{
		Close_port();
		if (show_errors)
			show_messagef(MSG_ERR, _("%s is not a valid serial port\n"), config.port);
		return FALSE;
	}

	if (!config.disable_port_lock)
	{
		if (flock(serial_port_fd, LOCK_EX | LOCK_NB) == -1)
		{
			Close_port();
			/* A lock conflict means another process is actively using the
			 * port: always report it, even on automatic (re)connect, as it
			 * is a hard failure the user needs to resolve. */
			show_messagef(MSG_ERR, _("Cannot lock port: %s\n"), g_strerror(errno));
			return FALSE;
		}
	}

	/* Allow 1/3 bit times wrong by the end of the first stop bit
	   to avoid failing due to rounding. */
	speed_margin = config.vitesse/(3U*(2U+(unsigned int)config.bits+(!!config.parite ? 1U : 0U)));
	serial_port_speed = set_port_baudrate(config.vitesse, serial_port_fd);

	/* These comparisons handle integer wraparound correctly. */
	if (serial_port_speed < config.vitesse - speed_margin ||
	    serial_port_speed - speed_margin > config.vitesse)
	{
		Close_port();
		if (show_errors)
			show_messagef(MSG_ERR, _("Unable to set baud rate %u"), config.vitesse);
		return FALSE;
	}

	tcgetattr(serial_port_fd, &termios_p);
	memcpy(&termios_save, &termios_p, sizeof(struct termios));

	switch(config.bits)
	{
	case 5:
		termios_p.c_cflag |= CS5;
		break;
	case 6:
		termios_p.c_cflag |= CS6;
		break;
	case 7:
		termios_p.c_cflag |= CS7;
		break;
	case 8:
		termios_p.c_cflag |= CS8;
		break;
	default:
		break;
	}
	switch(config.parite)
	{
	case 1:
		termios_p.c_cflag |= PARODD | PARENB;
		break;
	case 2:
		termios_p.c_cflag |= PARENB;
		break;
	default:
		break;
	}
	if(config.stops == 2)
		termios_p.c_cflag |= CSTOPB;
	termios_p.c_cflag |= CREAD;
	termios_p.c_iflag = IGNPAR | IGNBRK;
	switch(config.flux)
	{
	case 1:
		termios_p.c_iflag |= IXON | IXOFF;
		break;
	case 2:
#ifdef CRTSCTS
		termios_p.c_cflag |= CRTSCTS;
#endif
#ifdef CRTS_IFLOW
		termios_p.c_cflag |= CRTS_IFLOW;
#endif
#ifdef CCTS_OFLOW
		termios_p.c_cflag |= CCTS_OFLOW;
#endif
		break;
	default:
		termios_p.c_cflag |= CLOCAL;
		break;
	}
	termios_p.c_oflag = 0;
	termios_p.c_lflag = 0;
	termios_p.c_cc[VTIME] = 0;
	termios_p.c_cc[VMIN] = 1;
	tcsetattr(serial_port_fd, TCSANOW, &termios_p);
	tcflush(serial_port_fd, TCOFLUSH);
	tcflush(serial_port_fd, TCIFLUSH);

	/* Read initial modem status */
	{
		int initial_stat = 0;
		ioctl(serial_port_fd, TIOCMGET, &initial_stat);
		show_control_signals(initial_stat);
	}

	{
		GIOChannel *ch_in = g_io_channel_unix_new(serial_port_fd);
		callback_handler_in = g_io_add_watch_full(ch_in, 10, G_IO_IN | G_IO_HUP | G_IO_ERR,
		                      (GIOFunc)Lis_port, NULL, NULL);
		g_io_channel_unref(ch_in);
	}

	/* Try TIOCMIWAIT (event-driven); fall back to polling if driver doesn't support it.
	   SIGURG is used to interrupt the blocking ioctl on Close_port. */
	{
		struct sigaction sa;
		sa.sa_handler = modem_sigurg_noop;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(SIGURG, &sa, NULL);
	}
	modem_watch_fd = dup(serial_port_fd);
	if (modem_watch_fd != -1) {
		g_debug("modem status: starting TIOCMIWAIT thread (blocking in kernel until line changes)");
		if (pthread_create(&modem_tid, NULL, modem_thread_func,
		                   (void *)(intptr_t)modem_watch_fd) != 0) {
			g_debug("modem status: pthread_create failed, using %dms poll", POLL_DELAY);
			close(modem_watch_fd);
			modem_watch_fd = -1;
			modem_poll_source = g_timeout_add(POLL_DELAY, control_signals_read, NULL);
		}
	} else {
		g_debug("modem status: dup(fd) failed, using %dms poll", POLL_DELAY);
		modem_poll_source = g_timeout_add(POLL_DELAY, control_signals_read, NULL);
	}

	callback_activated = TRUE;

	Set_local_echo(term_conf.echo);

	/* Clear suspend-resume reconnect state only after a successful open. */
	device_monitor_clear_resume_reconnect();

	return TRUE;
}

void Close_port(void)
{
	if(serial_port_fd != -1)
	{
		if(callback_activated == TRUE)
		{
			g_source_remove(callback_handler_in);
			callback_activated = FALSE;
		}
		/* Stop modem monitor thread: SIGURG interrupts any in-progress
		   TIOCMIWAIT; closing modem_watch_fd ensures EBADF if SIGURG
		   arrived between two calls. */
		if (modem_tid != 0) {
			pthread_kill(modem_tid, SIGURG);
			if (modem_watch_fd != -1) {
				close(modem_watch_fd);
				modem_watch_fd = -1;
			}
			pthread_join(modem_tid, NULL);
			modem_tid = 0;
		}
		if (modem_poll_source != 0) {
			g_source_remove(modem_poll_source);
			modem_poll_source = 0;
		}
		if (!config.disable_port_lock)
			flock(serial_port_fd, LOCK_UN);
		tcsetattr(serial_port_fd, TCSANOW, &termios_save);
		tcflush(serial_port_fd, TCOFLUSH);
		tcflush(serial_port_fd, TCIFLUSH);
		close(serial_port_fd);
		serial_port_fd = -1;
	}
}

void Set_signals(guint param)
{
	int stat_;

	if(serial_port_fd == -1)
		return;

	if(ioctl(serial_port_fd, TIOCMGET, &stat_) == -1)
	{
		g_printerr("%s: %s\n", _("Control signals read set signals"), g_strerror(errno));
		return;
	}

	switch(param)
	{
	case 0: stat_ ^= TIOCM_DTR; break;
	case 1: stat_ ^= TIOCM_RTS; break;
	default: return;
	}

	if(ioctl(serial_port_fd, TIOCMSET, &stat_) == -1)
		g_printerr("%s: %s\n", _("TIOCMSET"), g_strerror(errno));

	show_control_signals(stat_);
}

void sendbreak(void)
{
	if(serial_port_fd == -1)
		return;
	else
		tcsendbreak(serial_port_fd, 0);
}

const gchar *get_port_string(void)
{
	static gchar buf[64];
	gchar parity;

	if(serial_port_fd == -1)
		return _("No open port");

	// 0: none, 1: odd, 2: even
	switch(config.parite)
	{
	case 1:
		parity = 'O';
		break;
	case 2:
		parity = 'E';
		break;
	default:
		parity = 'N';
	}

	/* "device  baud-bits-parity-stops" */
	g_snprintf(buf, sizeof(buf), "%.15s  %u-%d-%c-%d",
	           config.port, serial_port_speed,
	           config.bits, parity, config.stops);
	return buf;
}
