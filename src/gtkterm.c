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
#include <gdk/gdk.h>
#include <stdlib.h>

#include "widgets.h"
#include "serie.h"
#include "term_config.h"
#include "cmdline.h"
#include "parsecfg.h"
#include "buffer.h"
#include "macros.h"
#include "auto_config.h"

#include <config.h>
#include <glib/gi18n.h>

int main(int argc, char *argv[])
{
	gchar *message;

	config_file = g_strdup_printf("%s/.gtktermrc", getenv("HOME"));

	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	gtk_init(&argc, &argv);

	create_buffer();

	create_main_window();

	if(read_command_line(argc, argv) < 0)
	{
		delete_buffer();
		exit(1);
	}

	Config_port();

	message = get_port_string();
	Set_window_title(message);
	Set_status_message(message);
	g_free(message);

	add_shortcuts();

	set_view(ASCII_VIEW);

	gtk_main();

	delete_buffer();

	Close_port_and_remove_lockfile();

	return 0;
}
