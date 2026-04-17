/***********************************************************************/
/* buffer.c                                                            */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Management of a local buffer of data received                  */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.7 : removed (send)auto crlf stuff - (use macros instead)*/
/*      - 0.99.5 : Corrected segfault in case of buffer overlap        */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.98.4 : file creation by Julien                             */
/*                                                                     */
/***********************************************************************/

#include <glib.h>
#include <string.h>
#include "buffer.h"
#include "serial.h"

#include <config.h>
#include <glib/gi18n.h>
#include <time.h>

#define TIMESTAMP_SIZE 50

#include "config_file.h"

static int need_to_write_timestamp = 0;
static char *buffer = NULL;
static char *current_buffer;
static unsigned int pointer;
static int cr_received = 0;
char overlapped;

extern guint virt_col_pos;


void (*write_func)(const char *, size_t) = NULL;
void (*clear_func)(void) = NULL;

void create_buffer(void)
{
	if (buffer == NULL)
	{
		buffer = g_malloc(BUFFER_SIZE);
		clear_buffer();
	}
}

void delete_buffer(void)
{
	g_free(buffer);
	buffer = NULL;
	current_buffer = NULL;
	pointer = 0;
}

//assumes that buffer always has space for timestamp (TIMESTAMP_SIZE)
//buffer points to location where timestamp will be inserted
static unsigned int insert_timestamp(char *buf_out)
{
	unsigned int size = 0;

	if (config.timestamp)
	{
		struct timespec ts;
		struct tm tm;
		timespec_get(&ts, TIME_UTC);
		localtime_r(&ts.tv_sec, &tm);
		size = (unsigned int)g_snprintf(buf_out, TIMESTAMP_SIZE,
		    "[%d.%02dh.%02dm.%02ds.%03ld] ",
		    tm.tm_yday, tm.tm_hour, tm.tm_min, tm.tm_sec,
		    ts.tv_nsec / 1000000L);
	}

	return size;
}

void put_chars(const char *chars, size_t size, gboolean crlf_auto)
{
	// buffer must still be valid after cr conversion or adding timestamp
	// only pointer is copied below
	char out_buffer[(BUFFER_RECEPTION*2) + TIMESTAMP_SIZE];
	const char *characters;

	/* If the auto CR LF mode on, read the buffer to add \r before \n */
	if(crlf_auto || config.timestamp)
	{
		int i, out_size = 0;

		for (i=0; i<(int)size; i++)
		{
			if(crlf_auto)
			{
				if (chars[i] == '\r')
				{
					/* If the previous character was a CR too, insert a newline */
					if (cr_received)
					{
						out_buffer[out_size] = '\n';
						out_size++;
						need_to_write_timestamp = 1;
					}
					cr_received = 1;
				}
				else
				{
					if (chars[i] == '\n')
					{
						/* If we get a newline without a CR first, insert a CR */
						if (!cr_received)
						{
							out_buffer[out_size] = '\r';
							out_size++;
						}
					}
					else
					{
						/* If we receive a normal char, and the previous one was a
						   CR insert a newline */
						if (cr_received)
						{
							out_buffer[out_size] = '\n';
							out_size++;
							need_to_write_timestamp = 1;
						}
					}
					cr_received = 0;
				}
			} //if crlf_auto

			if(need_to_write_timestamp)
			{
				out_size += insert_timestamp(&out_buffer[out_size]);
				need_to_write_timestamp = 0;
			}

			if(chars[i] == '\n' )
			{
				need_to_write_timestamp = 1; //remember until we have a new character to print
			}

			//copy each character to new buffer
			out_buffer[out_size] = chars[i];
			out_size++; // increment for each stored character

		} // for

		// set "incoming" data pointer to new buffer containing all normal and
		// converted newline characters
		chars = out_buffer;
		size = out_size;
	} // if(crlf_auto || config.timestamp)

	if(buffer == NULL)
	{
		g_print(_("ERROR: Buffer is not initialized!\n"));
		return;
	}

	// when incoming size is larger than buffer, then just print the
	// last BUFFER_SIZE characters and ignore all other at begin of buffer
	if(size > BUFFER_SIZE)
	{
		characters = chars + (size - BUFFER_SIZE);
		size = BUFFER_SIZE;
	}
	else
		characters = chars;

	if((size + pointer) >= BUFFER_SIZE)
	{
		memcpy(current_buffer, characters, BUFFER_SIZE - pointer);
		chars = characters + BUFFER_SIZE - pointer;
		pointer = (unsigned int)(size - (BUFFER_SIZE - pointer));
		memcpy(buffer, chars, pointer);
		current_buffer = buffer + pointer;
		overlapped = 1;
	}
	else
	{
		memcpy(current_buffer, characters, size);
		pointer += size;
		current_buffer += size;
	}

	if(write_func != NULL)
		write_func(characters, size);
}

void write_buffer(void)
{
	if(write_func == NULL)
		return;

	if(overlapped == 0)
		write_func(buffer, pointer);
	else
	{
		write_func(current_buffer, BUFFER_SIZE - pointer);
		write_func(buffer, pointer);
	}
}

void write_buffer_with_func(void (*func)(const char *, size_t))
{
	void (*write_func_backup)(const char *, size_t);

	write_func_backup = write_func;
	write_func = func;
	write_buffer();
	write_func = write_func_backup;
}

void clear_buffer(void)
{
	if(clear_func != NULL)
		clear_func();

	if(buffer == NULL)
		return;

	overlapped = 0;
	memset(buffer, 0, BUFFER_SIZE);
	current_buffer = buffer;
	pointer = 0;
	cr_received = 0;

	virt_col_pos = 0;
}

void set_clear_func(void (*func)(void))
{
	clear_func = func;
}

void unset_clear_func(void (*func)(void))
{
	clear_func = NULL;
}

void set_display_func(void (*func)(const char *, size_t))
{
	write_func = func;
}

void unset_display_func(void (*func)(const char *, size_t))
{
	write_func = NULL;
}
