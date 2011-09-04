/***********************************************************************/
/* logging.h                                                           */
/* ---------                                                           */
/*                           GTKTerm Software                          */
/*                                 (c)                                 */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Log all data that GTKTerm sees to a file                       */
/*                                                                     */
/*   ChangeLog                                                         */
/*       0.99.7 - Logging added (Thanks to Brian Beattie)              */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <glib.h>

#include "widgets.h"
#include "serie.h"
#include "buffer.h"
#include "logging.h"

#include <config.h>
#include <glib/gi18n.h>

#define MAX_WRITE_ATTEMPTS 5

static gboolean	  Logging;
static gchar     *LoggingFileName;
static FILE      *LoggingFile;
static gchar     *logfile_default = NULL;

static gint OpenLogFile(gchar *filename)
{
    gchar *str;

    // open file and start logging
    if(!filename || (strcmp(filename, "") == 0))
    {
	str = g_strdup_printf(_("Filename error\n"));
	show_message(str, MSG_ERR);
	g_free(str);
	g_free(filename);
	return FALSE;
    }

    if(LoggingFile != NULL)
    {
	fclose(LoggingFile);
	LoggingFile = NULL;
	Logging = FALSE;
    }

    LoggingFileName = filename;

    LoggingFile = fopen(LoggingFileName, "a");
    if(LoggingFile == NULL)
    {
	str = g_strdup_printf(_("Cannot open file %s: %s\n"), LoggingFileName, strerror(errno));

	show_message(str, MSG_ERR);
	g_free(str);
	g_free(LoggingFileName);
    } else {
	logfile_default = g_strdup(LoggingFileName);
	Logging = TRUE;
    }

    return FALSE;
}

void logging_start(GtkAction *action, gpointer data)
{
    GtkWidget *file_select;
    gint retval;

    file_select = gtk_file_chooser_dialog_new(_("Log file selection"), GTK_WINDOW(Fenetre),
					      GTK_FILE_CHOOSER_ACTION_SAVE,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(file_select), TRUE);

    if(logfile_default != NULL)
    {
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_select), logfile_default);
    }

    retval = gtk_dialog_run(GTK_DIALOG(file_select));
    if(retval == GTK_RESPONSE_OK)
    {
       OpenLogFile(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_select)));
    }

    gtk_widget_destroy(file_select);

    toggle_logging_sensitivity(Logging);
    toggle_logging_pause_resume(Logging);
}

void logging_clear(void)
{
   if(LoggingFile == NULL)
   {
      return;
   }

   //Reopening with "w" will truncate the file
   LoggingFile = freopen(LoggingFileName, "w", LoggingFile);

   if (LoggingFile == NULL)
   {
      gchar *str = g_strdup_printf(_("Cannot open file %s: %s\n"), LoggingFileName, strerror(errno));
      show_message(str, MSG_ERR);
      g_free(str);
      g_free(LoggingFileName);
   }
}

void logging_pause_resume(void)
{
    if(LoggingFile == NULL) {
	return;
    }
    if(Logging == TRUE) {
	Logging = FALSE;
    } else {
	Logging = TRUE;
    }
    toggle_logging_pause_resume(Logging);
}

void logging_stop(void)
{
    if(LoggingFile == NULL) {
	return;
    }

    fclose(LoggingFile);
    LoggingFile = NULL;
    Logging = FALSE;
    g_free(LoggingFileName);
    LoggingFileName = NULL;

    toggle_logging_sensitivity(Logging);
    toggle_logging_pause_resume(Logging);
}

void log_chars(gchar *chars, guint size)
{
   guint writeAttempts = 0;
   guint bytesWritten = 0;

    /* if we are not logging exit */
    if(LoggingFile == NULL || Logging == FALSE) {
    	return;
    }

    while (bytesWritten < size)
    {
       if (writeAttempts < MAX_WRITE_ATTEMPTS)
       {
          bytesWritten += fwrite(&chars[bytesWritten], 1,
                                 size-bytesWritten, LoggingFile);
       }
       else
       {
           show_message(_("Failed to log data\n"), MSG_ERR);
          return;
       }
    }

    fflush(LoggingFile);
}
