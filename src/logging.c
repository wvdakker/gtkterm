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
#include <errno.h>
#include <string.h>

#include "interface.h"
#include "serial.h"
#include "buffer.h"
#include "logging.h"

#include <config.h>
#include <glib/gi18n.h>

#define MAX_WRITE_ATTEMPTS 5

static gboolean	  Logging;
static gchar     *LoggingFileName = NULL;
static FILE      *LoggingFile = NULL;
static gchar     *logfile_default = NULL;

static void close_log_file(void)
{
	if(LoggingFile != NULL)
	{
		fclose(LoggingFile);
		LoggingFile = NULL;
	}
	Logging = FALSE;
	g_free(LoggingFileName);
	LoggingFileName = NULL;
}

static void OpenLogFile(const gchar *filename)
{
	// open file and start logging
	if(!filename || !filename[0])
	{
		show_message(_("Filename error\n"), MSG_ERR);
		return;
	}

	close_log_file();

	LoggingFile = fopen(filename, "a");
	if(LoggingFile == NULL)
	{
		g_autofree gchar *str = g_strdup_printf(_("Cannot open file %s: %s\n"), filename, g_strerror(errno));
		show_message(str, MSG_ERR);
	}
	else
	{
		LoggingFileName = g_strdup(filename);
		g_free(logfile_default);
		logfile_default = g_strdup(filename);
		Logging = TRUE;
	}
}

static void on_log_file_response(GObject *source, GAsyncResult *result, gpointer data)
{
	GtkFileDialog *dialog;
	GError *error;
	GFile *file;
	gchar *filename;

	dialog = GTK_FILE_DIALOG(source);
	error = NULL;
	file = gtk_file_dialog_save_finish(dialog, result, &error);

	if (!file)
	{
		g_clear_error(&error);
		toggle_logging_sensitivity(Logging);
		toggle_logging_pause_resume(Logging);
		return;
	}

	filename = g_file_get_path(file);
	g_object_unref(file);
	OpenLogFile(filename);
	g_free(filename);

	toggle_logging_sensitivity(Logging);
	toggle_logging_pause_resume(Logging);
}

void logging_start(GSimpleAction *action, GVariant *param, gpointer data)
{
	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, _("Log to file"));
	gtk_file_dialog_set_modal(dialog, TRUE);

	if (logfile_default != NULL)
	{
		GFile *f = g_file_new_for_path(logfile_default);
		gtk_file_dialog_set_initial_file(dialog, f);
		g_object_unref(f);
	}

	gtk_file_dialog_save(dialog, GTK_WINDOW(Fenetre), NULL,
	                     on_log_file_response, NULL);
	g_object_unref(dialog);
}

void logging_clear(GSimpleAction *action, GVariant *param, gpointer data)
{
	if(LoggingFile == NULL)
		return;

	//Reopening with "w" will truncate the file
	LoggingFile = freopen(LoggingFileName, "w", LoggingFile);

	if (LoggingFile == NULL)
	{
		g_autofree gchar *str = g_strdup_printf(_("Cannot open file %s: %s\n"), LoggingFileName, g_strerror(errno));
		show_message(str, MSG_ERR);
		close_log_file();
	}
}

void logging_pause_resume(GSimpleAction *action, GVariant *param, gpointer data)
{
	if(LoggingFile == NULL)
	{
		return;
	}
	Logging = !Logging;
	toggle_logging_pause_resume(Logging);
}

void logging_stop(GSimpleAction *action, GVariant *param, gpointer data)
{
	if(LoggingFile == NULL)
	{
		return;
	}

	close_log_file();

	toggle_logging_sensitivity(Logging);
	toggle_logging_pause_resume(Logging);
}

void log_chars(const gchar *chars, guint size)
{
	guint writeAttempts = 0;
	guint bytesWritten = 0;

	/* if we are not logging exit */
	if(LoggingFile == NULL || Logging == FALSE)
	{
		return;
	}

	while (bytesWritten < size)
	{
		guint prev = bytesWritten;
		bytesWritten += (guint)fwrite(&chars[bytesWritten], 1,
		                       size-bytesWritten, LoggingFile);
		if (bytesWritten == prev && ++writeAttempts >= MAX_WRITE_ATTEMPTS)
		{
			show_message(_("Failed to log data\n"), MSG_ERR);
			return;
		}
	}

	fflush(LoggingFile);
}

void logging_cleanup(void)
{
	close_log_file(); /* frees LoggingFileName, closes LoggingFile */
	g_free(logfile_default);
	logfile_default = NULL;
}
