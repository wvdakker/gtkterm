/***********************************************************************/
/* baudrates.c                                                         */
/* -------                                                             */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Sorted list of "standard" baud rates defined in <termios.h>    */
/*      for any particular system.  The last entry will be { 0, B0 }.  */
/*                                                                     */
/***********************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <glib.h>

#include <config.h>

#include "serial.h"

const struct baudrate baudrate_list[] = {
#include "baudrates.h"
	{ 0, B0 }
};

const int baudrate_count =
	sizeof(baudrate_list) / sizeof(baudrate_list[0]) - 1;

const gboolean speed_t_is_sane = SPEED_T_IS_SANE;

speed_t find_standard_baudrate(unsigned int baud)
{
	size_t l = 0;
	size_t r = baudrate_count;

	while (l < r)
	{
		size_t i = (l+r) >> 1;
		if (baudrate_list[i].baud < baud)
			l = i+1;
		else if (baudrate_list[i].baud > baud)
			r = i;
		else
			return baudrate_list[i].speed;
	}

	return B0;
}

unsigned int speed_t_to_baud(speed_t speed)
{
	if (speed_t_is_sane)
	{
		unsigned int baud = speed;
		if (baud != speed)
			baud = 0;
		return baud;
	}
	else
	{
		size_t i;

		for (i = 0; i < baudrate_count; i++)
		{
			if (baudrate_list[i].speed == speed)
				break;
		}
		return baudrate_list[i].baud; /* 0 if end of list */
	}
}
