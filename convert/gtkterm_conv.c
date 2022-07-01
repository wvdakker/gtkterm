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
#include "parsecfg.h"

#include <config.h>

//! load old config file with parsecfg
//! Because we convert all sections we can walk trough all section numbers
extern int load_old_configuration_from_file (int);
extern cfgStruct cfg[];

//! Define external variables here
//! configuration for terminal window and serial port
display_config_t term_conf;
port_config_t port_conf;

//! This is the cli version of the one in gtkterm
void show_message (char * msg, int type) {
	i18n_printf ("%s\n", msg);
};

int main (int argc, char **argv) {

	int error = 0;
	int i;
	int section_count = 0;
	GKeyFile *configrc;

	//! Initialize for localization
	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	i18n_printf(_("\nGTKTerm version %s\n"), PACKAGE_VERSION);
	i18n_printf(_("\t (c) Julien Schmitt\n"));
	i18n_printf(_("\nThis program is released under the terms of the GPL V3 or later\n"));
	i18n_printf(_("GTKTerm_conv converts the 1.x resource file (.gtktermrc) to 2.0 resource structure.\n\n"));

	//! Check if the file exists
	config_file_init ();

	section_count  = cfgParse(g_file_get_path(config_file), cfg, CFG_INI);

	for (i = 0; i < section_count; i++)
		i18n_printf(_("Found section [%s]\n"), cfgSectionNumberToName(i));

	i18n_printf("\n");

	configrc  = g_key_file_new ();

	for (i = 0; i < section_count; i++) {

		i18n_printf(_("Converting section [%s]\n"), cfgSectionNumberToName(i));
		//! load old config file with parsecfg
		error = load_old_configuration_from_file (i);

		if (error == 0) {
			//! Copy the section into the '2.0' structure and save it
			copy_configuration(configrc, cfgSectionNumberToName(i));
			save_configuration_to_file(configrc, cfgSectionNumberToName(i));
		}
	}

	//! Dump all sections to cli
	for (i = 0; i < section_count; i++)
		dump_configuration_to_cli (cfgSectionNumberToName(i));

	g_key_file_unref (configrc);
}