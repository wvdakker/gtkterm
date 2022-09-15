/************************************************************************/
/* gtkterm_conv.c                                                       */
/* --------------                                                       */
/*           GTKTerm Software                                           */
/*                      (c) Julien Schmitt                              */
/*                                                                      */
/* -------------------------------------------------------------------  */
/*                                                                      */
/*   Purpose                                                            */
/*      Conversion v1 to v2 configuration                               */
/*                                                                      */
/*   ChangeLog                                                          */
/*      - 2.0 : Initial  file creation                                  */
/*                                                                      */
/* This GtkTerm is free software: you can redistribute it and/or modify	*/ 
/* it under the terms of the GNU  General Public License as published  	*/
/* by the Free Software Foundation, either version 3 of the License,   	*/
/* or (at your option) any later version.							   	*/
/*													                 	*/
/* GtkTerm is distributed in the hope that it will be useful, but	   	*/
/* WITHOUT ANY WARRANTY; without even the implied warranty of 		   	*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 			   	*/
/* See the GNU General Public License for more details.					*/
/*																	    */
/* You should have received a copy of the GNU General Public License 	*/
/* along with GtkTerm If not, see <https://www.gnu.org/licenses/>. 		*/
/*                                                                     	*/
/************************************************************************/

#include <stdio.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <pango/pango-font.h>

#include "gtkterm_struct.h"
#include "resource_file_conv.h"
#include "parsecfg.h"

#include <config.h>

//! load old config file with parsecfg
//! Because we convert all sections we can walk trough all section numbers
extern int load_old_configuration_from_file (int);
extern cfgStruct cfg[];

//! Define external variables here
//! configuration for terminal window and serial port
term_config_t term_conf;
port_config_t port_conf;

//! This is the cli version of the one in gtkterm
void show_message (char * msg, int type) {
	g_printf ("%s\n", msg);
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

	g_printf(_("\nGTKTerm version %s\n"), PACKAGE_VERSION);
	g_printf(_("\t (c) Julien Schmitt\n"));
	g_printf(_("\nThis program is released under the terms of the GPL V3 or later\n"));
	g_printf(_("GTKTerm_conv converts the 1.x resource file (.gtktermrc) to 2.0 resource structure.\n\n"));

	//! Check if the file exists
	config_file_init ();

	section_count  = cfgParse(g_file_get_path(config_file), cfg, CFG_INI);

	for (i = 0; i < section_count; i++)
		g_printf(_("Found section [%s]\n"), cfgSectionNumberToName(i));

	g_printf("\n");

	configrc  = g_key_file_new ();

	for (i = 0; i < section_count; i++) {

		g_printf(_("Converting section [%s]\n"), cfgSectionNumberToName(i));
		//! load old config file with parsecfg
		error = load_old_configuration_from_file (i);

		if (error == 0) {
			//! Copy the section into the '2.0' structure and save it
			copy_configuration(configrc, cfgSectionNumberToName(i));
		}
	}

	save_configuration_to_file(configrc);

	g_key_file_unref (configrc);
}