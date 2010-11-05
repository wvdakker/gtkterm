/***********************************************************************/
/* i18n.c                                                              */
/* ------                                                              */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      UTF-8 conversion to print in the console                       */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.5 : created strerror_utf8() function                    */
/*      - 0.99.2 : file creation by Julien                             */
/*                 function iconv_from_utf8_to_locale is based         */
/*                 on hddtemp 0.3 source code                          */
/*                                                                     */
/***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <iconv.h>
#include <langinfo.h>
#include <locale.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <glib.h>

#include "i18n.h"

static char *iconv_from_utf8_to_locale(const char *str, const char *fallback_string)
{
  const char *charset;

  iconv_t cd;
  size_t nconv;

  char *buffer, *old_buffer;
  char *buffer_ptr;
  char *string_ptr;
  char *string;

  const size_t buffer_inc = 80; // Increment buffer size in 80 bytes step
  size_t buffer_size;
  size_t buffer_size_left;
  size_t string_size;

  // Get the current charset
  charset = nl_langinfo(CODESET);

  if(strcmp(charset, "UTF-8") == 0)
    return g_strdup(str);

  // Open iconv descriptor
  cd = iconv_open(charset, "UTF-8");
  if (cd == (iconv_t) -1)
    return g_strdup(fallback_string);

  // Set up the buffer
  buffer_size = buffer_size_left = buffer_inc;
  buffer = (char *) malloc(buffer_size + 1);
  if (buffer == NULL)
    return g_strdup(fallback_string);
  buffer_ptr = buffer;
  string = g_strdup(str);
  string_ptr = string;
  string_size = strlen(string);
  // Do the conversion
  while (string_size != 0)
  {
    nconv = iconv(cd, &string_ptr, &string_size, &buffer_ptr, &buffer_size_left);
    if (nconv == (size_t) -1)
    {
      if (errno != E2BIG)                 // if translation error
      {
        iconv_close(cd);                  // close descriptor
        free(buffer);                     // free buffer
	free(string);
        return g_strdup(fallback_string); // and return fallback string
      }
      // increase buffer size
      buffer_size += buffer_inc;
      buffer_size_left = buffer_inc;
      old_buffer = buffer;
      buffer = (char *) realloc(buffer, buffer_size + 1);
      if (buffer == NULL)
	{
	  free(string);
	  return g_strdup(fallback_string);
	}
      buffer_ptr = (buffer_ptr - old_buffer) + buffer;
    }
  }
  *buffer_ptr = '\0';
  iconv_close(cd);
  free(string);

  return buffer;
}

int i18n_printf(const char *format, ...)
{
  char *new_format;
  int return_value;
  va_list args;

  new_format = iconv_from_utf8_to_locale(format, "");

  if(new_format != NULL)
    {
      va_start(args, format);
      return_value = vprintf(new_format, args);
      va_end(args);
      free(new_format);
    }
  else
    return_value = 0;

  return return_value;
}

int i18n_fprintf(FILE *stream, const char *format, ...)
{
  char *new_format;
  int return_value;
  va_list args;

  new_format = iconv_from_utf8_to_locale(format, "");

  if(new_format != NULL)
    {
      va_start(args, format);
      return_value = vfprintf(stream, new_format, args);
      va_end(args);
      free(new_format);
    }
  else
    return_value = 0;

  return return_value;
}

void i18n_perror(const char *s)
{
  char *conv_string;
  int errno_backup;

  errno_backup = errno;

  conv_string = iconv_from_utf8_to_locale(s, "");
  if(conv_string != NULL)
    {
      fprintf(stderr, "%s: %s\n", conv_string, strerror_utf8(errno_backup));
      free(conv_string);
    }
}

char *strerror_utf8(int errornum)
{
  char *utf8error; 
  
  utf8error = g_locale_to_utf8(strerror(errornum), -1, NULL, NULL, NULL);

  return utf8error;
}
