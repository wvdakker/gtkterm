/***********************************************************************/
/* gtkterm.c                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Main program file                                              */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.99.0 : added call to add_shortcuts()                       */
/*      - 0.98 : all GUI functions moved to widgets.c                  */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#include <signal.h>

#include "interface.h"
#include "serial.h"
#include "term_config.h"
#include "cmdline.h"
#include "buffer.h"
#include "macros.h"
#include "auto_config.h"
#include "config_file.h"
#include "device_monitor.h"
#include "user_signals.h"
#include "files.h"

#include <config.h>
#include <glib/gi18n.h>

typedef struct {
	int argc;
	char **argv;
} AppData;

static void on_shutdown(GtkApplication *app, gpointer user_data)
{
	delete_buffer();
	Close_port();
	device_monitor_stop();
	config_file_free();
	files_cleanup();
}

static void activate(GtkApplication *app, gpointer user_data)
{
	AppData *data = user_data;
	gchar *message;

	create_buffer();

	create_main_window(app);

	if(read_command_line(data->argc, data->argv) < 0)
	{
		g_application_quit(G_APPLICATION(app));
		return;
	}

	Config_port();
	ConfigFlags();

	message = get_port_string();
	Set_window_title(message);
	Set_status_message(message);
	g_free(message);

	set_view(ASCII_VIEW);

	device_monitor_start();

	user_signals_catch();
}

int main(int argc, char *argv[])
{
	AppData data = {argc, argv};
	GtkApplication *app;
	int status;

	config_file_init();
	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	app = gtk_application_new("org.gtk.gtkterm", G_APPLICATION_DEFAULT_FLAGS);
	g_application_set_resource_base_path(G_APPLICATION(app), "/org/gtk/gtkterm");
	g_signal_connect(app, "activate", G_CALLBACK(activate), &data);
	g_signal_connect(app, "shutdown", G_CALLBACK(on_shutdown), NULL);

	status = g_application_run(G_APPLICATION(app), 0, NULL);
	g_object_unref(app);

	/* Release Pango's global Cairo font map after GTK has fully shut down.
	 * This allows Pango (and fontconfig underneath it) to free their internal
	 * font caches, which would otherwise show as "definitely lost" in Valgrind
	 * and LeakSanitizer even though no gtkterm code itself leaked them. */
	pango_cairo_font_map_set_default(NULL);

	return status;
}
