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
/*      - 0.99.7 : removed auto crlf stuff - (use macros instead)      */
/*      - 0.99.5 : Corrected segfault in case of buffer overlap        */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.98.4 : file creation by Julien                             */
/*                                                                     */
/***********************************************************************/

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#include "gettext.h"
#include "i18n.h"
#include "serie.h"

static char *buffer = NULL;
static char *current_buffer;
static unsigned int pointer;
static int cr_received = 0;
char overlapped;

void (*write_func)(char *, unsigned int) = NULL;
void (*clear_func)(void) = NULL;

void create_buffer(void)
{
  if(buffer == NULL)
    {
      buffer = malloc(BUFFER_SIZE);
      clear_buffer();
    }
  return;
}

void delete_buffer(void)
{
  if(buffer != NULL)
    free(buffer);
  return;
}

void put_chars(char *chars, unsigned int size)
{
    char *characters;

    if(buffer == NULL)
    {
	i18n_printf(_("ERROR : Buffer is not initialized !\n"));
	return;
    }
  
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
	pointer = size - (BUFFER_SIZE - pointer);
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

void write_buffer_with_func(void (*func)(char *, unsigned int))
{
  void (*write_func_backup)(char *, unsigned int);
 
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
}

void set_clear_func(void (*func)(void))
{
  clear_func = func;
}

void unset_clear_func(void (*func)(void))
{
  clear_func = NULL;
}

void set_display_func(void (*func)(char *, unsigned int))
{
  write_func = func;
}

void unset_display_func(void (*func)(char *, unsigned int))
{
  write_func = NULL;
}

