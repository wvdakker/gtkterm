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

#include <gtk/gtk.h>
#include <glib.h>
#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "serial.h"
#include "term_config.h"
#include "i18n.h"

#include <config.h>
#include <glib/gi18n.h>

#ifdef HAVE_LINUX_SERIAL_H
#include <linux/serial.h>
#endif

struct termios termios_save;
int serial_port_fd = -1;

char* get_port_string(void)
{
	char* msg;
	char parity;

	if(serial_port_fd == -1)
	{
		msg = g_strdup(_("No open port"));
	}
	else
	{
		// 0: none, 1: odd, 2: even
		switch(config.parity)
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

		/* "GtkTerm: device  baud-bits-parity-stops"  */
		msg = g_strdup_printf("%.15s  %d-%d-%c-%d",
		                      config.port,
		                      config.speed,
		                      config.bits,
		                      parity,
		                      config.stops
		                     );
	}

	return msg;
}
