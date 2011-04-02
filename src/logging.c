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

GtkWidget	*Fenetre;
gboolean	 Logging;
gchar		*LoggingFileName;
FILE		*LoggingFile;
gchar           *logfile_default = NULL;

gint OpenLogFile(GtkFileChooser *file_select)
{
    gchar *str;

    // open file and start logging
    LoggingFileName = gtk_file_chooser_get_filename(file_select);

    if(!LoggingFileName || (strcmp(LoggingFileName, "") == 0))
    {
	str = g_strdup_printf(_("Filename error\n"));
	show_message(str, MSG_ERR);
	g_free(str);
	g_free(LoggingFileName);
	return FALSE;
    }

    if(LoggingFile != NULL)
    {
	fclose(LoggingFile);
	LoggingFile = NULL;
	Logging = FALSE;
    }

    LoggingFile = fopen(LoggingFileName, "a");
    if(LoggingFile == NULL)
    {
	str = g_strdup_printf(_("Can not open file '%s': %s\n"), LoggingFileName, strerror(errno));

	show_message(str, MSG_ERR);
	g_free(str);
	g_free(LoggingFileName);
    } else {
	logfile_default = g_strdup(LoggingFileName);
	Logging = TRUE;
    }

    return FALSE;
}

gint logging_start(GtkWidget *widget)
{
    GtkWidget *file_select;
    gint retval;

    file_select = gtk_file_chooser_dialog_new(_("Log file selection"), GTK_WINDOW(Fenetre),
					      GTK_FILE_CHOOSER_ACTION_OPEN,
					      _("Cancel"), GTK_RESPONSE_CANCEL,
					      _("OK"), GTK_RESPONSE_OK, NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(file_select), TRUE);

    if(logfile_default != NULL)
    {
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_select), logfile_default);
    }

    retval = gtk_dialog_run(GTK_DIALOG(file_select));
    if(retval == GTK_RESPONSE_OK)
    {
	OpenLogFile(GTK_FILE_CHOOSER(file_select));
    }
    gtk_widget_destroy(file_select);
    return FALSE;
}

void logging_pause(void)
{
    if(LoggingFile == NULL) {
	return;
    }
    if(Logging == TRUE) {
	Logging = FALSE;
    } else {
	Logging = TRUE;
    }
}

void logging_stop(void)
{
    if(LoggingFile == NULL) {
	show_message("Not Logging\n", MSG_ERR);
	return;
    }

    fclose(LoggingFile);
    LoggingFile = NULL;
    Logging = FALSE;
    g_free(LoggingFileName);
    LoggingFileName = NULL;
}

void log_chars(gchar *chars, unsigned int size)
{
    /* if we are not logging exit */
    if(LoggingFile == NULL || Logging == FALSE) {
    	return;
    }

    if(fwrite(chars, 1, size, LoggingFile) < size) {
    	show_message("log_chars fwrite failed\n", MSG_ERR);
    }

    fflush(LoggingFile);
}
