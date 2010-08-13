/***********************************************************************/
/* buffer.c                                                            */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                      julien@jls-info.com                            */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Management of a local buffer of data received                  */
/*                                                                     */
/*   ChangeLog                                                         */
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

static char *buffer = NULL;
static char *current_buffer;
static unsigned int pointer;

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

void put_chars(char *chars, unsigned int size, gboolean crlf_auto)
{
  char *characters;

  int pos;
  GString *buffer_tmp;
  gchar *in_buffer;

  buffer_tmp =  g_string_new(chars);
	  
  /* If the auto CR LF mode on, read the buffer to add \r before \n */ 

  if(crlf_auto)
    {
      in_buffer=buffer_tmp->str;
      
      in_buffer += size;
      for(pos=size; pos>0; pos--)
	{
	  in_buffer--;
	  
	  if(*in_buffer=='\n' && *(in_buffer-1) != '\r') 
	    {
	      g_string_insert_c(buffer_tmp, pos-1, '\r');
	      size += 1;
	    }

	  if(*in_buffer=='\r' && *(in_buffer+1) != '\n') 
	    {
	      g_string_insert_c(buffer_tmp, pos, '\n');
	      size += 1;
	    }
	}
    }

  chars = buffer_tmp->str;

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

