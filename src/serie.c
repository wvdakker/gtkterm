/***********************************************************************/
/* serie.c                                                             */
/* -------                                                             */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                      julien@jls-info.com                            */                      
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
#include <linux/serial.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>

#include "gettext.h"
#include "config.h"
#include "serie.h"
#include "widgets.h"
#include "fichier.h"
#include "buffer.h"
#include "i18n.h"

struct termios termios_save;
int serial_port_fd = -1;

guint callback_handler;
gboolean callback_activated = FALSE;
char lockfile[128] = {0};

gchar *str_err = NULL;

extern struct configuration_port config;

/* Local functions prototype */
gint create_lockfile(char *);
void remove_lockfile(void);
void Ferme_Port(void);
void Ouvre_Port(char *);

void Lis_port(gpointer data, gint source, GdkInputCondition condition)
{
    gint bytes_read;
    static gchar c[BUFFER_RECEPTION];
    guint i;

    bytes_read = BUFFER_RECEPTION;

    while(bytes_read == BUFFER_RECEPTION)
    {
	bytes_read = read(serial_port_fd, c, BUFFER_RECEPTION);
	if(bytes_read > 0)
	{

	    if(config.car != -1 && waiting_for_char == TRUE)
	    {
		i = 0;
		while(i < bytes_read)
		{
		    if(c[i] == config.car)
		    {
			waiting_for_char = FALSE;
			add_input();
			i = bytes_read;
		    }
		    i++;
		}
	    }
	}
	else if(bytes_read == -1)
	{
	    if(errno != EAGAIN)
		perror(config.port);
	}
    }
    return;
}


int Send_chars(char *string, int length)
{
    int bytes_written = 0;

    /* Normally it never happens, but it is better not to segfault ;) */
    if(length == 0)
	return 0;

    bytes_written = write(serial_port_fd, string, length);

    return bytes_written;
}

void Ouvre_Port(char *port)
{
    serial_port_fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
}

gchar *Config_port(void)
{
    struct termios termios_p;
    gchar *msg;

    Ferme_Port();
    remove_lockfile();

    Ouvre_Port(config.port);

    if(serial_port_fd == -1)
    {
	g_free(str_err);
	str_err = g_strdup_printf(_("Cannot open %s : %s\n"), config.port, strerror_utf8(errno));
	show_message(str_err, MSG_ERR);
	return NULL;
    }

    if(create_lockfile(config.port) == -1)
    {
	Ferme_Port();
	return NULL;
    }

    msg = g_strdup_printf("%s : %d,%d", 
			  config.port, 
			  config.vitesse,
			  config.bits);
    tcgetattr(serial_port_fd, &termios_p);
    memcpy(&termios_save, &termios_p, sizeof(struct termios));

    switch(config.vitesse)
    {
	case 300:
	    termios_p.c_cflag = B300;
	    break;
	case 600:
	    termios_p.c_cflag = B600;
	    break;
	case 1200:
	    termios_p.c_cflag = B1200;
	    break;
	case 2400:
	    termios_p.c_cflag = B2400;
	    break;
	case 4800:
	    termios_p.c_cflag = B4800;
	    break;
	case 9600:
	    termios_p.c_cflag = B9600;
	    break;
	case 19200:
	    termios_p.c_cflag = B19200;
	    break;
	case 38400:
	    termios_p.c_cflag = B38400;
	    break;
	case 57600:
	    termios_p.c_cflag = B57600;
	    break;
	case 115200:
	    termios_p.c_cflag = B115200;
	    break;

	default:
	    set_custom_speed(termios_p, config.vitesse, serial_port_fd);
    }

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
    }
    switch(config.parite)
    {
	case 1:
	    termios_p.c_cflag |= PARODD | PARENB;
	    msg = g_strdup_printf("%s,O", msg);
	    break;
	case 2:
	    termios_p.c_cflag |= PARENB;
	    msg = g_strdup_printf("%s,E", msg);
	    break;
	default:
	    msg = g_strdup_printf("%s,N", msg);
	    break;
    }
    if(config.stops == 2)
	termios_p.c_cflag |= CSTOPB;
    termios_p.c_cflag |= CREAD;
    msg = g_strdup_printf("%s,%d", msg, config.stops);
    termios_p.c_iflag = IGNPAR | IGNBRK;
    switch(config.flux)
    {
	case 1:
	    termios_p.c_iflag |= IXON | IXOFF;
	    msg = g_strdup_printf("%s - Xon/Xoff", msg);
	    break;
	case 2:
	    termios_p.c_cflag |= CRTSCTS;
	    msg = g_strdup_printf("%s - RTS/CTS", msg);
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

    callback_handler = gtk_input_add_full(serial_port_fd, GDK_INPUT_READ, (GdkInputFunction)Lis_port, NULL, NULL, NULL);
    callback_activated = TRUE;

    Set_local_echo(config.echo);

    return msg;
}

void configure_echo(gboolean echo)
{
    config.echo = echo;
}


void Ferme_Port(void)
{
    if(serial_port_fd != -1)
    {
	if(callback_activated == TRUE)
	{
	    gtk_input_remove(callback_handler);
	    callback_activated = FALSE;
	}
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
	i18n_perror(_("Control signals read"));
	return;
    }

    /* DTR */
    if(param == 0)
    {
	if(stat_ & TIOCM_DTR)
	    stat_ &= ~TIOCM_DTR;
	else
	    stat_ |= TIOCM_DTR;
	if(ioctl(serial_port_fd, TIOCMSET, &stat_) == -1)
	    i18n_perror(_("DTR write"));
    }
    /* RTS */
    else if(param == 1)
    {
	if(stat_ & TIOCM_RTS)
	    stat_ &= ~TIOCM_RTS;
	else
	    stat_ |= TIOCM_RTS;
	if(ioctl(serial_port_fd, TIOCMSET, &stat_) == -1)
	    i18n_perror(_("RTS write"));
    }
}

int lis_sig(void)
{
    static int stat = 0;
    int stat_read;

    if(serial_port_fd != -1)
    {
	if(ioctl(serial_port_fd, TIOCMGET, &stat_read) == -1)
	{
            /* Ignore EINVAL, as some serial ports 
	       genuinely lack these lines */
	    /* Thanks to Elie De Brauwer on ubuntu launchpad */
	    if (errno != EINVAL)
	    {
		i18n_perror(_("Control signals read"));
		Ferme_Port();
	    }

	    return -2;
	}

	if(stat_read == stat)
	    return -1;

	stat = stat_read;

	return stat;
    }
    return -1;
}

/*
 * Find out name to use for lockfile when locking tty.
 */
char *mbasename(char *s, char *res, int reslen)
{
    char *p;
  
    if (strncmp(s, "/dev/", 5) == 0) {
	/* In /dev */
	strncpy(res, s + 5, reslen - 1);
	res[reslen-1] = 0;
	for (p = res; *p; p++)
	    if (*p == '/') *p = '_';
    } else {
	/* Outside of /dev. Do something sensible. */
	if ((p = strrchr(s, '/')) == NULL)
	    p = s;
	else
	    p++;
	strncpy(res, p, reslen - 1);
	res[reslen-1] = 0;
    }

    return res;
}

gint create_lockfile(char *port)
{
    char buf[128];
    char *username;
    struct stat stt;
    int fd, n = 0;
    int pid;
    int mask;
    uid_t real_uid;
    gid_t real_gid;

    real_uid = getuid();
    real_gid = getgid();
    username = (getpwuid(real_uid))->pw_name;

    /* First see if the lock file directory is present. */
    if(P_LOCK[0] && stat(P_LOCK, &stt) == 0) 
	snprintf(lockfile, sizeof(lockfile), "%s/LCK..%s", P_LOCK, mbasename(port, buf, sizeof(buf)));
    else
	lockfile[0] = 0;

    if(lockfile[0] && (fd = open(lockfile, O_RDONLY)) >= 0) 
    {
	n = read(fd, buf, 127);
	close(fd);
	if(n > 0) 
	{
	    pid = -1;
	    if(n == 4)
		/* Kermit-style lockfile. */
		pid = *(int *)buf;
	    else {
		/* Ascii lockfile. */
		buf[n] = 0;
		sscanf(buf, "%d", &pid);
	    }
	    if(pid > 0 && kill((pid_t)pid, 0) < 0 && errno == ESRCH) 
	    {
		i18n_fprintf(stderr, _("Lockfile is stale. Overriding it..\n"));
		sleep(1);
		unlink(lockfile);
	    }
	    else
		n = 0;
        }

	if(n == 0) 
	{
	    i18n_fprintf(stderr, _("Device %s is locked.\n"), port);
	    lockfile[0] = 0;
	    return -1;
	}
    }

    if(lockfile[0]) 
    {
	/* Create lockfile compatible with UUCP-1.2 */
	mask = umask(022);
	if((fd = open(lockfile, O_WRONLY | O_CREAT | O_EXCL, 0666)) < 0) 
	{
	    i18n_fprintf(stderr, _("Cannot create lockfile. Sorry.\n"));
	    lockfile[0] = 0;
	    return -1;
        }
	(void)umask(mask);
	(void)chown(lockfile, real_uid, real_gid);
	snprintf(buf, sizeof(buf), "%10ld gtkterm %.20s\n", (long)getpid(), username);
	write(fd, buf, strlen(buf));
	close(fd);
    }

    return 0;
}

void remove_lockfile(void)
{
    if(lockfile[0])
	unlink(lockfile);
}

void Close_port_and_remove_lockfile(void)
{
    Ferme_Port();
    remove_lockfile();
}

void sendbreak(void)
{
    if(serial_port_fd == -1)
	return;
    else
	tcsendbreak(serial_port_fd, 0);
}

gint set_custom_speed(struct termios t, int speed, int port_fd)
{

    struct serial_struct ser;
    int arby;

    ioctl(port_fd, TIOCGSERIAL, &ser);
    ser.custom_divisor = ser.baud_base / speed;
    if(!(ser.custom_divisor)) 
	ser.custom_divisor = 1;

    arby = ser.baud_base / ser.custom_divisor;
    ser.flags &= ~ASYNC_SPD_MASK;
    ser.flags |= ASYNC_SPD_CUST;

    ioctl(port_fd, TIOCSSERIAL, &ser);
    t.c_cflag |= B38400;

    return 0;
}
