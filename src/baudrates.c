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

#include <stdlib.h>
#include <termios.h>
#include <glib.h>

#include <config.h>

#include "serial.h"

const struct baudrate baudrate_list[] = {
#include "baudrates.h"
};

const gsize baudrate_count = G_N_ELEMENTS(baudrate_list);
const gboolean speed_t_is_sane = SPEED_T_IS_SANE;

static int cmp_baud(const void *key, const void *elem)
{
	unsigned int k = *(const unsigned int *)key;
	unsigned int e = ((const struct baudrate *)elem)->baud;
	return k < e ? -1 : k > e ? 1 : 0;
}

speed_t find_standard_baudrate(unsigned int baud)
{
	const struct baudrate *p = bsearch(&baud, baudrate_list, baudrate_count,
	                                   sizeof baudrate_list[0], cmp_baud);
	return p ? p->speed : B0;
}


/* Return 0-based index of baud in baudrate_list[], or -1 if not found. */
int baudrate_find_index(unsigned int baud)
{
	const struct baudrate *p = bsearch(&baud, baudrate_list, baudrate_count,
	                                   sizeof baudrate_list[0], cmp_baud);
	return p ? (int)(p - baudrate_list) : -1;
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
		gsize i;

		for (i = 0; i < baudrate_count; i++)
		{
			if (baudrate_list[i].speed == speed)
				return baudrate_list[i].baud;
		}
		return 0;
	}
}
