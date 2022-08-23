/***********************************************************************/
/* cmdline.c                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Parse cmdline options                                          */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 2.0 : Switch to GOptionEntry (thanks to Jens Georg (phako))  */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.98.3 : modified for configuration file                     */
/*      - 0.98.2 : added --echo                                        */
/*      - 0.98 : file creation by Julien                               */
/*                                                                     */
/***********************************************************************/

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <config.h>

#include "defaults.h"
#include "gtkterm.h"
#include "resource_file.h"
#include "cmdline.h"
#include "serial.h"
#include "gtkterm_messages.h"

static bool on_remove_config (const char *name, const char *value, gpointer data,  GError **error) {
    int rc = CONF_ERROR_SUCCESS;

    /** Signal to load the configuration and dump it to the cli */
    g_signal_emit(GTKTERM_APP(data)->config, gtkterm_signals[SIGNAL_GTKTERM_REMOVE_SECTION], 0, value, &rc);

    g_printf ("%s\n", gtkterm_message(rc));

    g_application_quit (G_APPLICATION(data)); 

    return true;
}

static bool on_save_section (const char *name, const char *value, gpointer data,  GError **error) {
    int rc = CONF_ERROR_SUCCESS;

    /** Signal to load the configuration and dump it to the cli */
    g_signal_emit(GTKTERM_APP(data)->config, gtkterm_signals[SIGNAL_GTKTERM_SAVE_CONFIG], 0, value, &rc);

    g_printf ("%s\n", gtkterm_message(rc));

    g_application_quit (G_APPLICATION(data)); 

    return true;
}

static bool on_print_section (const char *name, const char *value, gpointer data,  GError **error) {
    int rc = CONF_ERROR_SUCCESS;

    /** Signal to load the configuration and dump it to the cli */
    g_signal_emit(GTKTERM_APP(data)->config, gtkterm_signals[SIGNAL_GTKTERM_PRINT_SECTION], 0, value, &rc);

    if (rc != CONF_ERROR_SUCCESS) {
        g_printf ("%s\n", gtkterm_message(rc));
    }

    g_application_quit (G_APPLICATION(data)); 

    return true;
}

static bool on_use_config (const char *name, const char *value, gpointer data,  GError **error) {
 
    if (strlen (value) < MAX_SECTION_LENGTH)  {

        GTKTERM_APP(data)->section = g_strdup ( value);
        return true;

    } else {

        g_printf (_("[CONFIGURATION]-name to long (max %d)\n\n"), MAX_SECTION_LENGTH);
        return false;
    }

    return true;
}
/*!
 * \brief GOptionEntry mappings
 * We use callback in GOptionEntry. So we can directly put them
 * in the Terminal configuration instead of handing over a pointer from the config.
 * 
 * \todo Update gtkterm.1
 */
static GOptionEntry gtkterm_config_options[] = {    
    {"show_config", 'v', 0, G_OPTION_ARG_CALLBACK, on_print_section, N_("Show configuration"), "[configuration]"}, 
    {"save_config", 's', 0, G_OPTION_ARG_CALLBACK, on_save_section, N_("Save configuration"), "[configuration]"},     
    {"remove_config", 'r', 0, G_OPTION_ARG_CALLBACK, on_remove_config, N_("Remove configuration"), "[configuration]"},
    {"use_config", 'u', 0, G_OPTION_ARG_CALLBACK, on_use_config, N_("Use configuration (must be first argument)"), "[configuration]"},    
    {NULL}
};

/*!
 * \brief Longname CLI options
 * For setting options we dont allow shortnames anymore.
 * This makes it easier to configure and more fault tolerant.
 */
static GOptionEntry gtkterm_term_options[] = {    
    {GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_DELAY], 'd', 0, G_OPTION_ARG_CALLBACK, on_set_config_options, N_("End of line delay in ms (default none)"), "<ms>"},
    {GtkTermConfigurationItems[CONF_ITEM_TERM_ECHO], 'e', 0, G_OPTION_ARG_CALLBACK, on_set_config_options,  N_("Local echo"),  "<on|off>"},
    {GtkTermConfigurationItems[CONF_ITEM_TERM_RAW_FILENAME], 'f', 0, G_OPTION_ARG_CALLBACK, on_set_config_options,  N_ ("Default file to send (default none)"), "<filename>"},
    {GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_CHAR], 'r', 0, G_OPTION_ARG_CALLBACK, on_set_config_options, N_("Wait for a special char at end of line (default none)"), "<character in HEX>"},
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

void gtkterm_add_cmdline_options (GtkTerm *app)
{
    char sz_context_summary[BUFFER_LENGTH];

    g_snprintf (sz_context_summary, BUFFER_LENGTH, "GTKTerm version %s (c) Julien Schmitt\n"
	        "This program is released under the terms of the GPL V3 or later.", PACKAGE_VERSION);
    g_application_set_option_context_summary (G_APPLICATION(app), sz_context_summary);

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
