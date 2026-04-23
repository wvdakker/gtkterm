/***********************************************************************/
/* files.c (fichier.c)                                                 */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Raw / text file transfer management                            */
/*      Ported to GTK4                                                 */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#include <stdio.h>
#include <errno.h>

#include "term_config.h"
#include "interface.h"
#include "serial.h"
#include "txqueue.h"
#include "buffer.h"

#include <glib/gi18n.h>

/* Global variables */
static GtkProgressBar *ProgressBar;
static GtkWindow *Window;
static FILE *Fic;
static gint last_pct = -1;
gchar *fic_defaut = NULL;

/* Local functions prototype */
static void on_file_progress(goffset written, goffset total, gpointer user_data);
static void on_file_done(gboolean success, gpointer user_data);
static gint close_all(gboolean success);
static gboolean on_file_transfer_close_request(GtkWindow *window, gpointer data);

/* ---- Send RAW file ---- */

/* Completes an open or save dialog, returns the selected path (caller owns it)
 * or NULL on cancel/error. */
static gchar *finish_file_dialog(GObject *source, GAsyncResult *result,
                                 gboolean is_save, const gchar *err_msg)
{
	gchar *fileName;
	GError *error = NULL;

	GFile *file = is_save
	    ? gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), result, &error)
	    : gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
	if (!file)
	{
		if (!g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
			show_message(MSG_ERR, err_msg);
		g_clear_error(&error);
		return NULL;
	}

	fileName = g_file_get_path(file);
	g_object_unref(file);
	return fileName;
}

/* Shared body for save-raw and save-ascii: opens fileName for writing,
 * drains the buffer through writer, then takes ownership of fileName. */
static void save_to_file(gchar *fileName, void (*writer)(const char *, gsize))
{
	Fic = fopen(fileName, "w");
	if (Fic == NULL)
	{
		show_messagef(MSG_ERR, _("Cannot open file %s: %s\n"),
		              fileName, g_strerror(errno));
		g_free(fileName);
	}
	else
	{
		g_free(fic_defaut);
		fic_defaut = fileName;
		write_buffer_with_func(writer);
		fclose(Fic);
	}
}

static void run_file_dialog(const gchar *title, gboolean is_save,
                            GAsyncReadyCallback callback)
{
	GtkFileDialog *dialog = gtk_file_dialog_new();

	gtk_file_dialog_set_title(dialog, title);
	gtk_file_dialog_set_modal(dialog, TRUE);

	if (fic_defaut != NULL)
	{
		GFile *f = g_file_new_for_path(fic_defaut);
		gtk_file_dialog_set_initial_file(dialog, f);
		g_object_unref(f);
	}

	if (is_save)
		gtk_file_dialog_save(dialog, Fenetre, NULL, callback, NULL);
	else
		gtk_file_dialog_open(dialog, Fenetre, NULL, callback, NULL);
	g_object_unref(dialog);
}

static void show_transfer_dialog(const gchar *title)
{
	GtkBuilderCScope *scope;
	GtkBuilder *builder;

	scope = GTK_BUILDER_CSCOPE(gtk_builder_cscope_new());
	gtk_builder_cscope_add_callback_symbols(scope,
	    "close_all",                        G_CALLBACK(close_all),
	    "on_file_transfer_close_request",   G_CALLBACK(on_file_transfer_close_request),
	    NULL);

	builder = gtk_builder_new();
	gtk_builder_set_scope(builder, GTK_BUILDER_SCOPE(scope));
	gtk_builder_add_from_resource(builder, "/org/gtk/gtkterm/file_transfer_dialog.ui", NULL);
	g_object_unref(scope);

	Window      = GTK_WINDOW(gtk_builder_get_object(builder, "file_transfer_window"));
	ProgressBar = GTK_PROGRESS_BAR(gtk_builder_get_object(builder, "file_transfer_progress"));

	gtk_window_set_title(Window, title);
	last_pct = -1;
	g_object_unref(builder);
	gtk_window_present(Window);
}

static void on_send_raw_response(GObject *source, GAsyncResult *result, gpointer data G_GNUC_UNUSED)
{
	gchar *fileName = finish_file_dialog(source, result, FALSE, _("Error opening file\n"));
	if (!fileName)
		return;

	g_free(fic_defaut);
	fic_defaut = fileName;

	gtk_label_set_text(GTK_LABEL(StatusBar), _("Transfer in progress..."));
	show_transfer_dialog(_("Transfer in progress..."));
	if (!txqueue_send_file(fileName, FALSE, on_file_progress, on_file_done, NULL))
	{
		show_messagef(MSG_ERR, _("Cannot open file %s: %s\n"), fileName, g_strerror(errno));
		gtk_label_set_text(GTK_LABEL(StatusBar), "");
		gtk_window_destroy(Window);
	}
}

void send_raw_file(GSimpleAction *action G_GNUC_UNUSED, GVariant *param G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	run_file_dialog(_("Send Binary File"), FALSE, on_send_raw_response);
}

static void on_send_text_response(GObject *source, GAsyncResult *result, gpointer data G_GNUC_UNUSED)
{
	gchar *fileName = finish_file_dialog(source, result, FALSE, _("Error opening file\n"));
	if (!fileName)
		return;

	g_free(fic_defaut);
	fic_defaut = fileName;

	gtk_label_set_text(GTK_LABEL(StatusBar), _("Transfer in progress..."));
	show_transfer_dialog(_("Transfer in progress..."));
	if (!txqueue_send_file(fileName, TRUE, on_file_progress, on_file_done, NULL))
	{
		show_messagef(MSG_ERR, _("Cannot open file %s: %s\n"), fileName, g_strerror(errno));
		gtk_label_set_text(GTK_LABEL(StatusBar), "");
		gtk_window_destroy(Window);
	}
}

void send_text_file(GSimpleAction *action G_GNUC_UNUSED, GVariant *param G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	run_file_dialog(_("Execute Script File"), FALSE, on_send_text_response);
}

/* File transfer progress and done callbacks ---- */

static void on_file_progress(goffset written, goffset total, gpointer data G_GNUC_UNUSED)
{
	gint pct = (total > 0) ? (gint)((written * 100) / total) : 0;
	if (pct != last_pct)
	{
		last_pct = pct;
		gtk_progress_bar_set_fraction(ProgressBar, pct / 100.0);
	}
}

static void on_file_done(gboolean success G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	close_all(success);
}

gint close_all(gboolean success)
{
	if (!success)
		show_message(MSG_ERR, _("Error sending file\n"));
	gtk_label_set_text(GTK_LABEL(StatusBar), "");
	gtk_window_destroy(Window);
	return FALSE;
}

/* Used as close-request handler: abort the transfer and let GTK destroy
 * the window itself (close-request returns FALSE to allow destruction). */
static gboolean on_file_transfer_close_request(GtkWindow *window G_GNUC_UNUSED,
	gpointer data G_GNUC_UNUSED)
{
	txqueue_abort();
	gtk_label_set_text(GTK_LABEL(StatusBar), "");
	return FALSE;
}

void write_file(const char *data, gsize size)
{
	fwrite(data, size, 1, Fic);
}

static void write_ascii_file(const char *data, gsize size)
{
	/* Use a static chunk buffer to avoid a heap allocation up to BUFFER_SIZE
	 * (128 KB) on every save.  Flush when full; stdio buffers the fwrites. */
	static char cleanbuff[4096];
	gsize newsize = 0;
	gsize x;
	for (x = 0; x < size; ++x)
	{
		if (data[x] > 0x1F || data[x] == 0x0A || data[x] == 0x0D)
		{
			cleanbuff[newsize++] = data[x];
			if (newsize == sizeof(cleanbuff))
			{
				fwrite(cleanbuff, newsize, 1, Fic);
				newsize = 0;
			}
		}
	}
	if (newsize > 0)
		fwrite(cleanbuff, newsize, 1, Fic);
}


/* ---- Save RAW file ---- */

static void on_save_raw_response(GObject *source, GAsyncResult *result, gpointer data G_GNUC_UNUSED)
{
	gchar *fileName = finish_file_dialog(source, result, TRUE, _("File error\n"));
	if (fileName)
		save_to_file(fileName, write_file);
}

void save_raw_file(GSimpleAction *action G_GNUC_UNUSED, GVariant *param G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	run_file_dialog(_("Save RAW File"), TRUE, on_save_raw_response);
}


/* ---- Save ASCII file ---- */

static void on_save_ascii_response(GObject *source, GAsyncResult *result, gpointer data G_GNUC_UNUSED)
{
	gchar *fileName = finish_file_dialog(source, result, TRUE, _("File error\n"));
	if (fileName)
		save_to_file(fileName, write_ascii_file);
}

void save_ascii_file(GSimpleAction *action G_GNUC_UNUSED, GVariant *param G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	run_file_dialog(_("Save ASCII File"), TRUE, on_save_ascii_response);
}

void files_cleanup(void)
{
	g_free(fic_defaut);
	fic_defaut = NULL;
}
