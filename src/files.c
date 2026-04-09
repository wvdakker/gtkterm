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
#include <string.h>

#include "term_config.h"
#include "interface.h"
#include "serial.h"
#include "buffer.h"

#include <config.h>
#include <glib/gi18n.h>

/* Global variables */
gint nb_car;
gint car_written;
gint current_buffer_position;
ssize_t bytes_read;
GtkAdjustment *adj;
GtkWidget *ProgressBar;
gint Fichier;
guint callback_handler;
gchar *fic_defaut = NULL;
GtkWidget *Window;
gboolean waiting_for_char = FALSE;
gboolean waiting_for_timer = FALSE;
gboolean input_running = FALSE;
gchar *str = NULL;
FILE *Fic;

/* Local functions prototype */
static gboolean ecriture(GIOChannel *src, GIOCondition cond, gpointer data);
gboolean timer(gpointer pointer);
gboolean idle(gpointer pointer);
void remove_input(void);
void add_input(void);
void write_file(const char *, size_t);
gint close_all(void);

extern struct configuration_port config;


/* ---- Send RAW file ---- */

static void on_send_raw_response(GObject *source, GAsyncResult *result, gpointer data)
{
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
	GError *error = NULL;
	GFile *file = gtk_file_dialog_open_finish(dialog, result, &error);

	if (!file)
	{
		if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			show_message(_("Error opening file\n"), MSG_ERR);
		g_clear_error(&error);
		return;
	}

	gchar *fileName = g_file_get_path(file);
	g_object_unref(file);

	{
		if (!fileName)
			return;

		if (!g_file_test(fileName, G_FILE_TEST_IS_REGULAR))
		{
			show_message(_("Error opening file\n"), MSG_ERR);
			g_free(fileName);
			return;
		}

		Fichier = open(fileName, O_RDONLY);
		if (Fichier != -1)
		{
			fic_defaut = g_strdup(fileName);
			g_autofree gchar *msg = g_strdup_printf(_("%s: transfer in progress..."), fileName);

			gtk_label_set_text(GTK_LABEL(StatusBar), msg);
			car_written = 0;
			current_buffer_position = 0;
			bytes_read = 0;
			nb_car = lseek(Fichier, 0L, SEEK_END);
			lseek(Fichier, 0L, SEEK_SET);

			{
				GtkBuilder *builder = gtk_builder_new_from_resource("/org/gtk/gtkterm/file_transfer_dialog.ui");
				GtkWidget *cancel_btn;

				Window      = GTK_WIDGET(gtk_builder_get_object(builder, "file_transfer_window"));
				ProgressBar = GTK_WIDGET(gtk_builder_get_object(builder, "file_transfer_progress"));
				cancel_btn  = GTK_WIDGET(gtk_builder_get_object(builder, "file_transfer_cancel"));

				gtk_window_set_title(GTK_WINDOW(Window), msg);
				g_signal_connect_swapped(cancel_btn, "clicked",
				                         G_CALLBACK(close_all), NULL);
				g_signal_connect(GTK_WIDGET(Window), "close-request",
				                 G_CALLBACK(close_all), NULL);

				gtk_window_set_modal(GTK_WINDOW(Window), TRUE);					g_object_unref(builder);				gtk_window_present(GTK_WINDOW(Window));
			}

			add_input();
		}
		else
		{
			g_autofree gchar *msg = g_strdup_printf(_("Cannot read file %s: %s\n"),
			                                         fileName, g_strerror(errno));
			show_message(msg, MSG_ERR);
		}
		g_free(fileName);
	}
}

void send_raw_file(GSimpleAction *action, GVariant *param, gpointer data)
{
	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, _("Send RAW File"));
	gtk_file_dialog_set_modal(dialog, TRUE);

	if (fic_defaut != NULL)
	{
		GFile *f = g_file_new_for_path(fic_defaut);
		gtk_file_dialog_set_initial_file(dialog, f);
		g_object_unref(f);
	}

	gtk_file_dialog_open(dialog, GTK_WINDOW(Fenetre), NULL,
	                     on_send_raw_response, NULL);
	g_object_unref(dialog);
}

static gboolean ecriture(GIOChannel *src, GIOCondition cond, gpointer data)
{
	static gchar buffer[BUFFER_EMISSION];
	static gchar *current_buffer;
	static gint bytes_to_write;
	ssize_t bytes_written;
	gchar *car;

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar),
	                              (gfloat)car_written / (gfloat)nb_car);

	if (car_written < nb_car)
	{
		if (current_buffer_position == bytes_read)
		{
			bytes_read = read(Fichier, buffer, BUFFER_EMISSION);
			current_buffer_position = 0;
			current_buffer = buffer;
			bytes_to_write = (gint)bytes_read;
		}

		if (current_buffer == NULL)
		{
			g_free(str);
			str = g_strdup_printf(_("Error sending file\n"));
			show_message(str, MSG_ERR);
			close_all();
			return G_SOURCE_REMOVE;
		}

		car = current_buffer;

		if (config.delai != 0 || config.car != -1)
		{
			bytes_to_write = current_buffer_position;
			while (*car != LINE_FEED && bytes_to_write < bytes_read)
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
			g_free(str);
			str = g_strdup_printf(_("Error sending file: %s\n"), strerror(errno));
			show_message(str, MSG_ERR);
			close_all();
			return G_SOURCE_REMOVE;
		}

		car_written += (gint)bytes_written;
		current_buffer_position += (gint)bytes_written;
		current_buffer += bytes_written;

		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar),
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

gboolean timer(gpointer pointer)
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
		callback_handler = g_io_add_watch_full(
			g_io_channel_unix_new(serial_port_fd),
			10,
			G_IO_OUT,
			ecriture,
			NULL, NULL);
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
	gtk_window_destroy(GTK_WINDOW(Window));

	return FALSE;
}

void write_file(const char *data, size_t size)
{
	fwrite(data, size, 1, Fic);
}

static void write_ascii_file(const char *data, size_t size)
{
	char *cleanbuff = g_malloc(size);
	int newsize = 0;
	for (size_t x = 0; x < size; ++x)
	{
		if (data[x] > 0x1F || data[x] == 0x0A || data[x] == 0x0D)
			cleanbuff[newsize++] = data[x];
	}
	fwrite(cleanbuff, (size_t)newsize, 1, Fic);
	g_free(cleanbuff);
}


/* ---- Save RAW file ---- */

static void on_save_raw_response(GObject *source, GAsyncResult *result, gpointer data)
{
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
	GError *error = NULL;
	GFile *file = gtk_file_dialog_save_finish(dialog, result, &error);

	if (!file)
	{
		if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			show_message(_("File error\n"), MSG_ERR);
		g_clear_error(&error);
		return;
	}

	gchar *fileName = g_file_get_path(file);
	g_object_unref(file);

	Fic = fopen(fileName, "w");
	if (Fic == NULL)
	{
		g_autofree gchar *msg = g_strdup_printf(_("Cannot open file %s: %s\n"),
		                                         fileName, g_strerror(errno));
		show_message(msg, MSG_ERR);
	}
	else
	{
		fic_defaut = g_strdup(fileName);
		write_buffer_with_func(write_file);
		fclose(Fic);
	}
	g_free(fileName);
}

void save_raw_file(GSimpleAction *action, GVariant *param, gpointer data)
{
	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, _("Save RAW File"));
	gtk_file_dialog_set_modal(dialog, TRUE);

	if (fic_defaut != NULL)
	{
		GFile *f = g_file_new_for_path(fic_defaut);
		gtk_file_dialog_set_initial_file(dialog, f);
		g_object_unref(f);
	}

	gtk_file_dialog_save(dialog, GTK_WINDOW(Fenetre), NULL,
	                     on_save_raw_response, NULL);
	g_object_unref(dialog);
}


/* ---- Save ASCII file ---- */

static void on_save_ascii_response(GObject *source, GAsyncResult *result, gpointer data)
{
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
	GError *error = NULL;
	GFile *file = gtk_file_dialog_save_finish(dialog, result, &error);

	if (!file)
	{
		if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			show_message(_("File error\n"), MSG_ERR);
		g_clear_error(&error);
		return;
	}

	gchar *fileName = g_file_get_path(file);
	g_object_unref(file);

	Fic = fopen(fileName, "w");
	if (Fic == NULL)
	{
		g_autofree gchar *msg = g_strdup_printf(_("Cannot open file %s: %s\n"),
		                                         fileName, g_strerror(errno));
		show_message(msg, MSG_ERR);
	}
	else
	{
		fic_defaut = g_strdup(fileName);
		write_buffer_with_func(write_ascii_file);
		fclose(Fic);
	}
	g_free(fileName);
}

void save_ascii_file(GSimpleAction *action, GVariant *param, gpointer data)
{
	GtkFileDialog *dialog = gtk_file_dialog_new();
	gtk_file_dialog_set_title(dialog, _("Save ASCII File"));
	gtk_file_dialog_set_modal(dialog, TRUE);

	if (fic_defaut != NULL)
	{
		GFile *f = g_file_new_for_path(fic_defaut);
		gtk_file_dialog_set_initial_file(dialog, f);
		g_object_unref(f);
	}

	gtk_file_dialog_save(dialog, GTK_WINDOW(Fenetre), NULL,
	                     on_save_ascii_response, NULL);
	g_object_unref(dialog);
}
