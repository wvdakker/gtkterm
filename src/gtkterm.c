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

#include "interface.h"
#include "serial.h"
#include "term_config.h"
#include "cmdline.h"
#include "buffer.h"
#include "macros.h"
#include "config_file.h"
#include "device_monitor.h"
#include "user_signals.h"
#include "files.h"
#include "logging.h"

#include <config.h>
#include <glib/gi18n.h>

static void on_shutdown(GtkApplication *app G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
	delete_buffer();
	Close_port();
	device_monitor_stop();
	remove_shortcuts();
	logging_cleanup();
	interface_cleanup();
	config_file_free();
	files_cleanup();
}

static void on_new_window_action(GSimpleAction *action G_GNUC_UNUSED,
                                 GVariant *param G_GNUC_UNUSED,
                                 gpointer user_data G_GNUC_UNUSED)
{
	GError *err = NULL;
	const gchar *argv[] = { "gtkterm", NULL };

	if (!g_spawn_async(NULL, (gchar **)argv, NULL,
	                   G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err))
	{
		g_warning("Failed to open new window: %s", err->message);
		g_error_free(err);
	}
}

static void activate(GtkApplication *app, gpointer user_data G_GNUC_UNUSED)
{
	create_buffer();

	create_main_window(app);
	interface_apply_term_config();

	Config_port();
	ConfigFlags();

	update_port_status();

	set_view(ASCII_VIEW);

	device_monitor_start();

	user_signals_catch();
}

int main(int argc, char *argv[])
{
	GtkApplication *app;
	int status;

	config_file_init();

	if (Load_configuration_from_file("default") == -1)
		Hard_default_configuration(); /* first run: no config file yet */
	if (read_command_line(argc, argv) < 0)
		return 0;

	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	app = gtk_application_new("org.gtk.gtkterm", G_APPLICATION_NON_UNIQUE);
	g_application_set_resource_base_path(G_APPLICATION(app), "/org/gtk/gtkterm");
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
	g_signal_connect(app, "shutdown", G_CALLBACK(on_shutdown), NULL);

	/* Register new-window action so GNOME Shell shows it in the dash
	 * context menu while the app is running (it reads GActions via D-Bus
	 * rather than the desktop file when an instance is live). */
	{
		GSimpleAction *nw = g_simple_action_new("new-window", NULL);
		g_signal_connect(nw, "activate", G_CALLBACK(on_new_window_action), NULL);
		g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(nw));
		g_object_unref(nw);
	}

	status = g_application_run(G_APPLICATION(app), 0, NULL);
	g_object_unref(app);

	/* Release Pango's global Cairo font map after GTK has fully shut down.
	 * This allows Pango (and fontconfig underneath it) to free their internal
	 * font caches, which would otherwise show as "definitely lost" in Valgrind
	 * and LeakSanitizer even though no gtkterm code itself leaked them. */
	pango_cairo_font_map_set_default(NULL);

	return status;
}
