#include <stdio.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <pango/pango-font.h>

#include "serial.h"
#include "term_config.h"
#include "interface.h"
#include "resource_file.h"
#include "i18n.h"

#include <config.h>

// load old config file with parsecfg
extern int load_old_configuration_from_file (char *);

// Define external variables here
// configuration for terminal window and serial port
display_config_t term_conf;
port_config_t port_conf;

// This is the cli version of the one in gtkterm
void show_message (char * msg, int type) {
	i18n_printf ("%s\n", msg);
};

int main (int argc, char **argv) {

	char *section = "default";
	int error = 0;
	GKeyFile *configrc;

	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	i18n_printf(_("\nGTKTerm version %s\n"), PACKAGE_VERSION);
	i18n_printf(_("\t (c) Julien Schmitt\n"));
	i18n_printf(_("\nThis program is released under the terms of the GPL V3 or later\n"));
	i18n_printf(_("GTKTerm_conv converts the 1.x resource file (.gtktermrc) to 2.0 rc structure.\n\n"));

	// Check if the file exists
	config_file_init ();

	// load old config file with parsecfg and put output on cli
	error = load_old_configuration_from_file (section);

	if (error == 0) {
		configrc  = g_key_file_new ();

		copy_configuration(configrc, section);
		save_configuration_to_file(configrc, section);
		dump_configuration_to_cli (section);

		g_key_file_unref (configrc);
	}
}