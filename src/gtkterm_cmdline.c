/************************************************************************/
/* gtkterm_cmdline.c                                                    */
/* -----------------                                                    */
/*           GTKTerm Software                                           */
/*                      (c) Julien Schmitt                              */
/*                                                                      */
/* -------------------------------------------------------------------  */
/*                                                                      */
/*   Purpose                                                            */
/*      Parse cmdline options                                           */
/*                                                                      */
/*   ChangeLog                                                          */
/*      - 2.0    : Gtk4 port (also thanks to Jens George)               */
/*      - 0.99.2 : Internationalization                                 */
/*      - 0.98.3 : modified for configuration file                      */
/*      - 0.98.2 : added --echo                                         */
/*      - 0.98 : file creation by Julien                                */
/*                                                                     	*/
/* This GtkTerm is free software: you can redistribute it and/or modify	*/ 
/* it under the terms of the GNU  General Public License as published  	*/
/* by the Free Software Foundation, either version 3 of the License,   	*/
/* or (at your option) any later version.							   	*/
/*																	   	*/
/* GtkTerm is distributed in the hope that it will be useful, but	   	*/
/* WITHOUT ANY WARRANTY; without even the implied warranty of 		   	*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 			   	*/
/* See the GNU General Public License for more details.					*/
/*																		*/
/* You should have received a copy of the GNU General Public License 	*/
/* along with GtkTerm If not, see <https://www.gnu.org/licenses/>. 		*/
/*                                                                     	*/
/************************************************************************/

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <config.h>

#include "gtkterm_defaults.h"
#include "gtkterm.h"
#include "gtkterm_window.h"
#include "gtkterm_terminal.h"
#include "gtkterm_serial_port.h"
#include "gtkterm_buffer.h"
#include "gtkterm_configuration.h"
#include "gtkterm_cmdline.h"
#include "gtkterm_serial_port.h"

/**
 * @brief  Removes a configuration sectons
 * 
 * The functions emits a signal which is connected to the config remove function.
 * After removing, the g_application_quit is called for exiting the application (we only want
 * to remove the section, not to start up GTKTerm).
 * 
 * @param name Not used.
 * 
 * @param value The section we want to remove.
 * 
 * @param data The application app. 
 * 
 * @param error Error (not used). 
 * 
 * @return  true on succes (we will not get there).
 * 
 */
static bool on_remove_config (const char *name, const char *value, gpointer data,  GError **error) {
    int rc = GTKTERM_CONFIGURATION_SUCCESS;

    /** Signal to load the configuration and dump it to the cli */
    g_signal_emit(GTKTERM_APP(data)->config, gtkterm_signals[SIGNAL_GTKTERM_REMOVE_SECTION], 0, value, &rc);

    g_printf ("%s\n", gtkterm_configuration_get_error (GTKTERM_APP(data)->config)->message);

    g_application_quit (G_APPLICATION(data)); 

    return true;
}

/**
 * @brief  Saves a configuration sectons
 * 
 * The functions emits a signal which is connected to the config save function.
 * After saving, the g_application_quit is called for exiting the application (we only want
 * to save the section, not to start up GTKTerm).
 * If we want to save cli options we have to put the save option as last parameter.
 * 
 * @param name Not used.
 * 
 * @param value The section we want to save.
 * 
 * @param data The application app. 
 * 
 * @param error Error (not used). 
 * 
 * @return  true on succes (we will not get there).
 * 
 */
static bool on_save_section (const char *name, const char *value, gpointer data,  GError **error) {
    int rc = GTKTERM_CONFIGURATION_SUCCESS;

    /** Signal to save the configuration and dump it to the cli */
    g_signal_emit(GTKTERM_APP(data)->config, gtkterm_signals[SIGNAL_GTKTERM_SAVE_CONFIG], 0, &rc);

    g_printf ("%s\n", gtkterm_configuration_get_error (GTKTERM_APP(data)->config)->message);

    g_application_quit (G_APPLICATION(data)); 

    return true;
}

/**
 * @brief  Prints a configuration sectons
 * 
 * The functions emits a signal which is connected to the config print function.
 * After printing, the g_application_quit is called for exiting the application (we only want
 * to print the section, not to start up GTKTerm)
 * 
 * @param name Not used.
 * 
 * @param value The section we want to print.
 * 
 * @param data The application app. 
 * 
 * @param error Error (not used). 
 * 
 * @return  true on succes (we will not get there).
 * 
 */
static bool on_print_section (const char *name, const char *value, gpointer data,  GError **error) {
    int rc = GTKTERM_CONFIGURATION_SUCCESS;

    /** Signal to load the configuration and dump it to the cli */
    g_signal_emit(GTKTERM_APP(data)->config, gtkterm_signals[SIGNAL_GTKTERM_PRINT_SECTION], 0, value, &rc);

    if (rc != GTKTERM_CONFIGURATION_SUCCESS) {
        g_printf ("%s\n", gtkterm_configuration_get_error (GTKTERM_APP(data)->config)->message);
    }

    g_application_quit (G_APPLICATION(data)); 

    return true;
}

/**
 * @brief  List all configurations
 * 
 * The functions emits a signal which is connected to list all configurations.
 * After printing, the g_application_quit is called for exiting the application (we only want
 * to list the configs, not to start up GTKTerm)
 * 
 * @param name Not used.
 * 
 * @param value Not used.
 * 
 * @param data The application app. 
 * 
 * @param error Error (not used). 
 * 
 * @return  true on succes (we will not get there).
 * 
 */
static bool on_list_config (const char *name, const char *value, gpointer data,  GError **error) {
    int rc = GTKTERM_CONFIGURATION_SUCCESS;

    /** Signal to list the comfigurations and dump it to the cli */
    g_signal_emit(GTKTERM_APP(data)->config, gtkterm_signals[SIGNAL_GTKTERM_LIST_CONFIG], 0, &rc);

    if (rc != GTKTERM_CONFIGURATION_SUCCESS) {
        g_printf ("%s\n", gtkterm_configuration_get_error (GTKTERM_APP(data)->config)->message);
    }

    g_application_quit (G_APPLICATION(data)); 

    return true;
}

/**
 * @brief  Sets the use of a configuration section.
 * 
 * This is used as input for config options or starting GTKTerm with the
 * section active.
 * 
 * @param name Not used.
 * 
 * @param value The section we want to use.
 * 
 * @param data The application app. 
 * 
 * @param error Error (not used). 
 * 
 * @return  true on succes (continues startup). False if the configurationname is too long.
 * 
 */
static bool on_use_config (const char *name, const char *value, gpointer data,  GError **error) {

    if (strlen (value) < MAX_SECTION_LENGTH)  {

        GTKTERM_APP(data)->section = g_strdup ( value);

    } else {

        g_printf (_("[CONFIGURATION]-name to long (max %d)\n\n"), MAX_SECTION_LENGTH);
        return false;
    }

    return true;
}

/**
 * @brief GOptionEntry mappings.
 * 
 * We use callback in GOptionEntry. So we can directly put them
 * in the Terminal configuration instead of handing over a pointer from the config.
 * 
 * \todo Update gtkterm.1.
 * 
 */
static GOptionEntry gtkterm_config_options[] = {    
    {"show_config", 'V', 0, G_OPTION_ARG_CALLBACK, on_print_section, N_("Show configuration"), "[configuration]"}, 
    {"save_config", 'S', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, on_save_section, N_("Save configuration"), NULL},     
    {"remove_config", 'R', 0, G_OPTION_ARG_CALLBACK, on_remove_config, N_("Remove configuration"), "[configuration]"},
    {"use_config", 'U', 0, G_OPTION_ARG_CALLBACK, on_use_config, N_("Use configuration (must be first argument)"), "[configuration]"},
    {"list_config", 'L', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, on_list_config, N_("List all configurations"), NULL},   
    {NULL}
};

/**
 * @brief Longname CLI options
 *
 */
static GOptionEntry gtkterm_term_options[] = {    
    {GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_DELAY], 'd', 0, G_OPTION_ARG_CALLBACK, on_set_config_options, N_("End of line delay in ms (default none)"), "<ms>"},
    {GtkTermConfigurationItems[CONF_ITEM_TERM_ECHO], 'e', 0, G_OPTION_ARG_CALLBACK, on_set_config_options,  N_("Local echo"),  "<on|off>"},
    {GtkTermConfigurationItems[CONF_ITEM_TERM_RAW_FILENAME], 'f', 0, G_OPTION_ARG_CALLBACK, on_set_config_options,  N_ ("Default file to send (default none)"), "<filename>"},
    {GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_CHAR], 'r', 0, G_OPTION_ARG_CALLBACK, on_set_config_options, N_("Wait for a special char at end of line (default none)"), "<character in HEX>"},
    {GtkTermConfigurationItems[CONF_ITEM_TERM_ROWS], 'o', 0, G_OPTION_ARG_CALLBACK, on_set_config_options, N_ ("Terminal rows (default 25)"), "<rows>"},
    {GtkTermConfigurationItems[CONF_ITEM_TERM_COLS], 'c', 0, G_OPTION_ARG_CALLBACK, on_set_config_options, N_ ("Terminal cols (default 80)"), "<cols>"},
    {GtkTermConfigurationItems[CONF_ITEM_TERM_AUTO_CR], 'g', 0, G_OPTION_ARG_CALLBACK, on_set_config_options, N_ ("Auto LF (default off)"), "<on|off>"},
    {GtkTermConfigurationItems[CONF_ITEM_TERM_AUTO_LF], 'h', 0, G_OPTION_ARG_CALLBACK, on_set_config_options, N_ ("Auto CR (default off)"), "<on|off>"},        
    {NULL}
};

static GOptionEntry gtkterm_port_options[] = {    
    {GtkTermConfigurationItems[CONF_ITEM_SERIAL_PARITY], 'a', 0, G_OPTION_ARG_CALLBACK, on_set_config_options, N_("Parity(default = none)"), "<odd|even|none>"},    
    {GtkTermConfigurationItems[CONF_ITEM_SERIAL_BITS], 'b', 0, G_OPTION_ARG_CALLBACK, on_set_config_options, N_("Number of bits (default 8)"), "<7|8> "},
    {GtkTermConfigurationItems[CONF_ITEM_SERIAL_PORT], 'p', 0, G_OPTION_ARG_CALLBACK, on_set_config_options,  N_("Serial port device (default /dev/ttyS0)"), "[device]"},
    {GtkTermConfigurationItems[CONF_ITEM_SERIAL_BAUDRATE], 's', 0, G_OPTION_ARG_CALLBACK, on_set_config_options, N_("Serial port baudrate(default 9600bps)"), "[baudrate]",}, 
    {GtkTermConfigurationItems[CONF_ITEM_SERIAL_STOPBITS], 't', 0, G_OPTION_ARG_CALLBACK, on_set_config_options,  N_("Number of stopbits (default 1)"), "<1|2>"},
    {GtkTermConfigurationItems[CONF_ITEM_SERIAL_FLOW_CONTROL], 'w', 0, G_OPTION_ARG_CALLBACK, on_set_config_options, N_("Flow control (default none)"), "<Xon|RTS|RS485|none>"},
    {GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX], 'x', 0, G_OPTION_ARG_CALLBACK, on_set_config_options, N_("For RS485, time in ms before transmit with RTS on"), "[ms]"},
    {GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX], 'y', 0, G_OPTION_ARG_CALLBACK, on_set_config_options, N_("For RS485, time in ms after transmit with RTS on"), "[ms]"},
    {GtkTermConfigurationItems[CONF_ITEM_SERIAL_DISABLE_PORT_LOCK], 'l', 0, G_OPTION_ARG_CALLBACK, on_set_config_options,  N_("Disable lock serial port "), "<on|off>"},   
    {NULL}
};

/**
 * @brief Add the commandline options.
 * 
 * Commandline options are grouped. So each group is created.
 * Each group has app as parameter passed through the callback.
 * 
 * @param app The application
 * 
 */
void gtkterm_add_cmdline_options (GtkTerm *app)
{
    char sz_context_summary[BUFFER_LENGTH];

    g_snprintf (sz_context_summary, BUFFER_LENGTH, "GTKTerm version %s (c) Julien Schmitt\n"
	        "This program is released under the terms of the GPL V3 or later.", PACKAGE_VERSION);
    g_application_set_option_context_summary (G_APPLICATION(app), sz_context_summary);

    /** Pass app as data to the new created group */
    app->g_term_group = g_option_group_new ("terminal", N_("Terminal options"), N_("Options for configuring terminal"), GTKTERM_APP(app), NULL);
    app->g_port_group = g_option_group_new ("port", N_("Port options"), N_("Options for configuring the port"), GTKTERM_APP(app), NULL);
    app->g_config_group = g_option_group_new ("configuration", N_("Configuration options"), N_("Options for maintaining the configuration (default = default)"), GTKTERM_APP(app), NULL);    

    g_option_group_add_entries (app->g_term_group, gtkterm_term_options);
    g_option_group_add_entries (app->g_port_group, gtkterm_port_options);
    g_option_group_add_entries (app->g_config_group, gtkterm_config_options); 

    g_application_add_option_group (G_APPLICATION(app), app->g_term_group);
    g_application_add_option_group (G_APPLICATION(app), app->g_port_group);
    g_application_add_option_group (G_APPLICATION(app), app->g_config_group);    
} 
