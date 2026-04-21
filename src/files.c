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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "term_config.h"
#include "interface.h"
#include "serial.h"
#include "buffer.h"

#include <glib/gi18n.h>

/* Global variables */
static gssize nb_car;
static gsize car_written;
static gsize current_buffer_position;
static gssize bytes_read;
static GtkProgressBar *ProgressBar;
static gint Fichier = -1;
static guint callback_handler;
gchar *fic_defaut = NULL;
static GtkWindow *Window;
gboolean waiting_for_char = FALSE;
static gboolean waiting_for_timer = FALSE;
static gboolean input_running = FALSE;
static FILE *Fic;

/* Local functions prototype */
static gboolean timer(gpointer pointer);
static void remove_input(void);
void add_input(void);
static gint close_all(void);
static gboolean on_file_transfer_close_request(GtkWindow *window, gpointer data);

extern struct configuration_port config;


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
	g_object_unref(builder);
	gtk_window_present(Window);
}

static void on_send_raw_response(GObject *source, GAsyncResult *result, gpointer data G_GNUC_UNUSED)
{
	gchar *fileName = finish_file_dialog(source, result, FALSE, _("Error opening file\n"));
	if (!fileName)
		return;

	Fichier = open(fileName, O_RDONLY);
	if (Fichier != -1)
	{
		g_free(fic_defaut);
		fic_defaut = fileName;
		/* Single concurrent transfer; static avoids a heap alloc on every send. */
		static gchar msg[4096 + 48];
		g_snprintf(msg, sizeof(msg), _("%s: transfer in progress..."), fileName);

		gtk_label_set_text(GTK_LABEL(StatusBar), msg);
		car_written = 0;
		current_buffer_position = 0;
		bytes_read = 0;
		nb_car = lseek(Fichier, 0L, SEEK_END);
		if (nb_car == -1 || lseek(Fichier, 0L, SEEK_SET) == -1)
		{
			show_messagef(MSG_ERR, _("Cannot seek in file %s: %s\n"),
			              fileName, g_strerror(errno));
			close(Fichier);
			Fichier = -1;
			return;
		}

		show_transfer_dialog(msg);
		add_input();
	}
	else
	{
		show_messagef(MSG_ERR, _("Cannot read file %s: %s\n"),
		              fileName, g_strerror(errno));
		g_free(fileName);
	}
}

void send_raw_file(GSimpleAction *action G_GNUC_UNUSED, GVariant *param G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	run_file_dialog(_("Send RAW File"), FALSE, on_send_raw_response);
}

static gboolean ecriture(GIOChannel *src G_GNUC_UNUSED, GIOCondition cond G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	static gchar buffer[BUFFER_EMISSION];
	static gchar *current_buffer;
	static gsize bytes_to_write;
	gssize bytes_written;
	gchar *car;

	gtk_progress_bar_set_fraction(ProgressBar, 
	                              (gfloat)car_written / (gfloat)nb_car);

	if (car_written < (gsize)nb_car)
	{
		if (current_buffer_position == (gsize)bytes_read)
		{
			bytes_read = read(Fichier, buffer, BUFFER_EMISSION);
			current_buffer_position = 0;
			current_buffer = buffer;
			bytes_to_write = (gsize)bytes_read;
		}

		if (current_buffer == NULL)
		{
			show_message(MSG_ERR, _("Error sending file\n"));
			close_all();
			return G_SOURCE_REMOVE;
		}

		car = current_buffer;

		if (config.delai != 0 || config.car != -1)
		{
			bytes_to_write = current_buffer_position;
			while (*car != LINE_FEED && bytes_to_write < (gsize)bytes_read)
			{
				car++;
				bytes_to_write++;
			}
			if (*car == LINE_FEED)
				bytes_to_write++;
		}

		bytes_written = send_serial(current_buffer,
		                            bytes_to_write - current_buffer_position);

		if (bytes_written == -1)
		{
			show_messagef(MSG_ERR, _("Error sending file: %s\n"), g_strerror(errno));
			close_all();
			return G_SOURCE_REMOVE;
		}

		car_written += (gsize)bytes_written;
		current_buffer_position += (gsize)bytes_written;
		current_buffer += bytes_written;

		gtk_progress_bar_set_fraction(ProgressBar,
		                              (gfloat)car_written / (gfloat)nb_car);

		if (config.delai != 0 && *car == LINE_FEED)
		{
			remove_input();
			g_timeout_add(config.delai, (GSourceFunc)timer, NULL);
			waiting_for_timer = TRUE;
		}
		else if (config.car != -1 && *car == LINE_FEED)
		{
			remove_input();
			waiting_for_char = TRUE;
		}
	}
	else
	{
		close_all();
	}
	return G_SOURCE_CONTINUE;
}

gboolean timer(gpointer pointer G_GNUC_UNUSED)
{
	if (waiting_for_timer == TRUE)
	{
		add_input();
		waiting_for_timer = FALSE;
	}
	return FALSE;
}

void add_input(void)
{
	if (input_running == FALSE)
	{
		input_running = TRUE;
		GIOChannel *channel = g_io_channel_unix_new(serial_port_fd);
		callback_handler = g_io_add_watch_full(
			channel,
			10,
			G_IO_OUT,
			ecriture,
			NULL, NULL);
		g_io_channel_unref(channel);
	}
}

void remove_input(void)
{
	if (input_running == TRUE)
	{
		g_source_remove(callback_handler);
		input_running = FALSE;
	}
}

gint close_all(void)
{
	remove_input();
	waiting_for_char = FALSE;
	waiting_for_timer = FALSE;
	gtk_label_set_text(GTK_LABEL(StatusBar), "");
	close(Fichier);
	Fichier = -1;
	gtk_window_destroy(Window);

	return FALSE;
}

/* Used as close-request handler: do cleanup but do NOT call gtk_window_destroy,
 * since GTK destroys the window itself after close-request returns FALSE. */
static gboolean on_file_transfer_close_request(GtkWindow *window G_GNUC_UNUSED,
	gpointer data G_GNUC_UNUSED)
{
	remove_input();
	waiting_for_char = FALSE;
	waiting_for_timer = FALSE;
	gtk_label_set_text(GTK_LABEL(StatusBar), "");
	close(Fichier);
	Fichier = -1;
	return FALSE; /* let GTK destroy the window */
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
