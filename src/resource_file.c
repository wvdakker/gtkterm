/***********************************************************************
 * resource_config.c                                 
 * -----------------                                    
 *           GTKTerm Software                              
 *                      (c) Julien Schmitt  
 *                                              
 * ------------------------------------------------------------------- 
 *                                           
 *   \brief Purpose                               
 *      Save and load configuration from resource file          
 *                                              
 *   ChangeLog                                
 *      - 2.0 : Remove parsecfg. Switch to GKeyFile  
 *              Migration done by Jens Georg (phako)       
 *                                                                
 ***********************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <pango/pango-font.h>

#include <config.h>

#include "gtkterm.h"
#include "serial.h"
#include "terminal.h"
#include "defaults.h"
#include "resource_file.h"
#include "interface.h"
#include "macros.h"

//! Default configuration filename
#define CONFIGURATION_FILENAME ".gtktermrc"		//! Name of the resource file
#define BUFFER_LENGTH			256				//! Bufferlength for strings.

//! Used configuration options to hold consistency between load/save functions
const char GtkTermConfigurationItems [][CONF_ITEM_LENGTH] = {
		"port",
		"baudrate",
		"bits",
		"stopbits",
		"parity",
		"flow_control",
		"term_wait_delay",
		"term_wait_char",
		"rs485_rts_time_before_tx",
		"rs485_rts_time_after_tx",
		"macros",
		"term_raw_filename",		
		"term_echo",
		"term_crlfauto",
		"disable_port_lock",
		"term_font",
		"term_show_timestamp",
		"term_block_cursor",				
		"term_show_cursor",
		"term_rows",
		"term_columns",
		"term_scrollback",
		"term_visual_bell",
		"term_foreground_red",
		"term_foreground_green",
		"term_foreground_blue",
		"term_foreground_alpha",
		"term_background_red",
		"term_background_green",
		"term_background_blue",
		"term_background_alpha",
};

G_BEGIN_DECLS

typedef struct {
   	GKeyFile *key_file;         //! The memory loaded keyfile
	GFile *config_file;         //! The config file

} GtkTermConfigurationPrivate;

struct _GtkTermConfiguration {
 
    GObject parent_instance;
};

struct _GtkTermConfigurationClass {
	
    GObjectClass parent_class;
};

G_DEFINE_TYPE_WITH_PRIVATE  (GtkTermConfiguration, gtkterm_configuration, G_TYPE_OBJECT)

static int gtkterm_configuration_load_config ();

//! Callback functions for signals
static int gtkterm_configuration_load_keyfile (GtkTermConfiguration *, gpointer);
static int gtkterm_configuration_save_keyfile (GtkTermConfiguration *, gpointer);
static int gtkterm_configuration_print_section (GtkTermConfiguration *, gpointer, gpointer);
static int gtkterm_configuration_remove_section (GtkTermConfiguration *, gpointer, gpointer);
static term_config_t *gtkterm_configuration_load_terminal_config (GtkTermConfiguration *, gpointer, gpointer);
static port_config_t *gtkterm_configuration_load_serial_config (GtkTermConfiguration *, gpointer, gpointer);

void gtkterm_configuration_default_configuration (GtkTermConfigurationPrivate *, char *);
void gtkterm_configuration_validate(GtkTermConfigurationPrivate *, char *);

//! For internal conversion only
static void set_color(GdkRGBA *, float, float, float, float);

int check_keyfile (GtkTermConfiguration *self, char *section) {
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private (self);

	//! Load keyfile if it is nog loaded yet
	if (priv->key_file == NULL) {
		if ( gtkterm_configuration_load_keyfile (self, NULL) < 0) {
			g_printf ("Failed to load configuration file: %s", g_file_get_path (priv->config_file));

			return -1;
		}
	}

	//! Check if the <section> exists in the key file.
 	if (!g_key_file_has_group (priv->key_file, section)) {
		char *string = NULL;

 		string = g_strdup_printf(_("No section [%s] in configuration file\n"), section);
 		show_message(string, MSG_ERR);
 		g_free(string);
 
 		return -1;
 	}

	return 0;
}

static void gtkterm_configuration_class_constructed (GObject *object) {
	GtkTermConfiguration *self = GTKTERM_CONFIGURATION(object);
 
	g_signal_connect (self, "config_print", G_CALLBACK(gtkterm_configuration_print_section), NULL);
	g_signal_connect (self, "config_remove", G_CALLBACK(gtkterm_configuration_remove_section), NULL);		
	g_signal_connect (self, "config_load", G_CALLBACK(gtkterm_configuration_load_keyfile), NULL);
	g_signal_connect (self, "config_save", G_CALLBACK(gtkterm_configuration_save_keyfile), NULL);
	g_signal_connect (self, "config_terminal", G_CALLBACK(gtkterm_configuration_load_terminal_config), NULL);
	g_signal_connect (self, "config_serial", G_CALLBACK(gtkterm_configuration_load_serial_config), NULL);		

	G_OBJECT_CLASS (gtkterm_configuration_parent_class)->constructed (object);
}

static void gtkterm_configuration_class_init (GtkTermConfigurationClass *class) {
    GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->constructed = gtkterm_configuration_class_constructed;
}

static void gtkterm_configuration_init (GtkTermConfiguration *self) {
	
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private (self);

	//! Initialize to NULL so we can detect if it is loaded.
	priv->config_file = NULL;
	priv->key_file = NULL;	
}

static int gtkterm_configuration_load_config (GtkTermConfiguration *self) {

	//! @brief
	//! Old location of configuration file was $HOME/.gtktermrc
	//! New location is $XDG_CONFIG_HOME/.gtktermrc
	//!
	//! If configuration file exists at new location, use that one.
	//! Otherwise, if file exists at old location, move file to new location.
	//!
	//! Version 2.0: Because we have to use gtkterm_conv, the file is always at
	//! the user directory. So we can skip eventually moving the file.
	//!
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private (self);

	GFile *config_file_old = g_file_new_build_filename(getenv("HOME"), CONFIGURATION_FILENAME, NULL);
	priv->config_file = g_file_new_build_filename(g_get_user_config_dir(), CONFIGURATION_FILENAME, NULL);

	 if (!g_file_query_exists(priv->config_file, NULL) && g_file_query_exists(config_file_old, NULL))
	 	g_file_move(config_file_old, priv->config_file, G_FILE_COPY_NONE, NULL, NULL, NULL, NULL);

	return 0;
}

//! @brief
//! Load the key file into memory.
//! Note: all sections are loaded.
static int gtkterm_configuration_load_keyfile (GtkTermConfiguration *self, gpointer user_data) {
	
	GError *error = NULL;
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);

	if (priv->config_file == NULL)
		gtkterm_configuration_load_config(self);

	priv->key_file = g_key_file_new ();

	if (!g_key_file_load_from_file (priv->key_file, g_file_get_path (priv->config_file), G_KEY_FILE_NONE, &error)) {
	 	g_debug ("Failed to load configuration file: %s", error->message);
	 	g_error_free (error);
	 	g_key_file_unref (priv->key_file);

	 	return -1;
	 }

	return 0;
}

//! @brief
//! Save keyfile to file (all sections are saved)
static int gtkterm_configuration_save_keyfile (GtkTermConfiguration *self, gpointer user_data)
{
	GError *error = NULL;
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);	

	if (priv->config_file == NULL) {
	 	g_debug ("No keyfile loaded. Nothing to save file.");

		return -1;
	}

 	if (!g_key_file_save_to_file (priv->key_file, g_file_get_path(priv->config_file), &error))
 	{
 		g_debug ("Failed to save configuration file: %s", error->message);
 		g_error_free (error);

		return -1;
 	}

	return 0;
 }

//! @brief: Dumps the section to the command line 
//! We will use this with auto package testing within Debian
static int gtkterm_configuration_print_section (GtkTermConfiguration *self, gpointer data, gpointer user_data) {
	char *section = (char *) data;
	gsize nr_of_strings;
	char **macrostring;
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);

	if (check_keyfile (self, section))
		return -1;

	g_printf (_("Configuration [%s] loaded from file\n"), section);

	//! Print the serial port items
	g_printf (_("\nSerial port settings:\n"));

 	//! Load all key file items into memory so we can use it if we open an terminal window
	//! Print serial port items
	g_printf (_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_PORT], g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_PORT], NULL));
	g_printf (_("%-24s : %ld\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_BAUDRATE], (unsigned long)g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BAUDRATE], NULL));
	g_printf (_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_BITS], g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BITS], NULL));
	g_printf (_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_STOPBITS], g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_STOPBITS], NULL));
	g_printf (_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_PARITY], g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_PARITY], NULL));
	g_printf (_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_FLOW_CONTROL], g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_FLOW_CONTROL], NULL));
	g_printf (_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX], g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX], NULL));
	g_printf (_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX], g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX], NULL));
	g_printf (_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_DISABLE_PORT_LOCK], g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_DISABLE_PORT_LOCK], NULL));

	//! Print the terminal items
	g_printf (_("\nTerminal settings:\n"));
	g_printf (_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_FONT], g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FONT], NULL));
	g_printf (_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_RAW_FILENAME], g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_RAW_FILENAME], NULL));
	g_printf (_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_ECHO], g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_ECHO], NULL));
	g_printf (_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_CRLF_AUTO], g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_CRLF_AUTO], NULL) );
	g_printf (_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_DELAY], g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_DELAY], NULL));
	g_printf (_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_CHAR], g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_CHAR], NULL));
	g_printf (_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_TIMESTAMP], g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_TIMESTAMP], NULL));
	g_printf (_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_BLOCK_CURSOR], g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BLOCK_CURSOR], NULL));
	g_printf (_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_SHOW_CURSOR], g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_SHOW_CURSOR], NULL));
	g_printf (_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_ROWS], g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_ROWS], NULL));
	g_printf (_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_COLS], g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_COLS], NULL));
	g_printf (_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_SCROLLBACK], g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_SCROLLBACK], NULL));
	g_printf (_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_VISUAL_BELL], g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_VISUAL_BELL], NULL));
	g_printf (_("%-24s : %f\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_RED], g_key_file_get_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_RED], NULL));
	g_printf (_("%-24s : %f\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_BLUE], g_key_file_get_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_BLUE], NULL));
	g_printf (_("%-24s : %f\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_GREEN], g_key_file_get_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_GREEN], NULL));
	g_printf (_("%-24s : %f\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_ALPHA], g_key_file_get_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_ALPHA], NULL));
	g_printf (_("%-24s : %f\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_RED], g_key_file_get_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_RED], NULL));
	g_printf (_("%-24s : %f\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_BLUE], g_key_file_get_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_BLUE], NULL));
	g_printf (_("%-24s : %f\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_GREEN], g_key_file_get_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_GREEN], NULL));
	g_printf (_("%-24s : %f\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_ALPHA], g_key_file_get_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_ALPHA], NULL));

 	//! Convert the stringlist to macros. Existing shortcuts will be deleted from convert_string_to_macros
 	macrostring = g_key_file_get_string_list (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_MACROS], &nr_of_strings, NULL);
 	convert_string_to_macros (macrostring, nr_of_strings);
 	g_strfreev(macrostring);

	//! ... and the macro's
 	g_printf (_("\nMacros:\n"));
 	g_printf (_(" Nr  Shortcut  Command\n"));

	if (macro_count() == 0)
		g_printf ("No macros defined in this section\n");
	else {
		for (int i = 0; i < macro_count(); i++) 
			g_printf ("[%2d] %-8s  %s\n", i, macros[i].shortcut, macros[i].action);
	}

	gtkterm_configuration_validate (priv, section);

	return 0;
}

//! @brief
//! Remove a section from the GKeyFile 
//! If it is the active section then switch back to default.
//! If it is the default section then create a new 'default' default section
static int gtkterm_configuration_remove_section (GtkTermConfiguration *self, gpointer data, gpointer user_data)
{
	char *string = NULL;
	char *section = (char *) data;
	GError *error = NULL;
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);

	if (check_keyfile (self, section))
		return -1;

	//! If we remove the DEFAULT_SECTION then create a new one
	if (!g_strcmp0 ((const char *) section, (const char *)&DEFAULT_SECTION)) {
		gtkterm_configuration_default_configuration(priv, section);
	}
	else {
		//! TODO: signal terminals to reload.

		//! Remove the group from GKeyFile
		if (!g_key_file_remove_group (priv->key_file, section, &error)) {

			g_debug ("Failed to remove section: %s", error->message);
			g_error_free (error);

			return -1;
		}
	}

	gtkterm_configuration_save_keyfile(self, user_data);	

	string = g_strdup_printf(_("Configuration [%s] removed\n"), section);
	show_message(string, MSG_WRN);
	g_free(string);

	return 0;
}

//! Load the configuration from <section> into the term config
//! If it does not exists it creates one from the defaults
static term_config_t *gtkterm_configuration_load_terminal_config (GtkTermConfiguration *self, gpointer data, gpointer user_data) {
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);
	term_config_t *term_conf;
	char *section = (char *)data;
	char *key_str = NULL;
	int key_value = 0;

	if (check_keyfile (self, section))
		return NULL;

	term_conf = g_malloc0 (sizeof (term_config_t));

	term_conf->delay = g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_DELAY], NULL);
	term_conf->rows = g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_ROWS], NULL);
	term_conf->columns = g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_COLS], NULL);
	term_conf->scrollback = g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_SCROLLBACK], NULL);

	key_str = g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BLOCK_CURSOR], NULL);
 	if (key_str != NULL) {
 		term_conf->block_cursor = g_ascii_strcasecmp (key_str, "true") ? true : false;

		g_free (key_str);
 	}

	key_str = g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_SHOW_CURSOR], NULL);
	if (key_str != NULL) {
 		term_conf->show_cursor = g_ascii_strcasecmp (key_str, "true") ? true : false;

		g_free (key_str);
 	}
	
	key_str = g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_CRLF_AUTO], NULL);
	if (key_str != NULL) {
 		term_conf->crlfauto = g_ascii_strcasecmp (key_str, "true") ? true : false;

		g_free (key_str);
 	}

	key_str = g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_ECHO], NULL);
	if (key_str != NULL) {
 		term_conf->echo = g_ascii_strcasecmp (key_str, "true") ? true : false;

		g_free (key_str);
 	}
		
	key_str = g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_TIMESTAMP], NULL);
	if (key_str != NULL) {
 		term_conf->timestamp = g_ascii_strcasecmp (key_str, "true") ? true : false;

		g_free (key_str);
 	}
	 	
	key_str = g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_VISUAL_BELL], NULL);
	if (key_str != NULL) {
 		term_conf->visual_bell = g_ascii_strcasecmp (key_str, "true") ? true : false;

		g_free (key_str);
 	}

 	key_value = g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_CHAR], NULL);
 	term_conf->char_queue = key_value ? (signed char) key_value : -1;

	set_color (&term_conf->foreground_color,
				g_key_file_get_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_RED], NULL),
				g_key_file_get_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_GREEN], NULL),
				g_key_file_get_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_BLUE], NULL),
				g_key_file_get_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_ALPHA], NULL));

	set_color (&term_conf->background_color,
				g_key_file_get_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_RED], NULL),
				g_key_file_get_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_GREEN], NULL),
				g_key_file_get_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_BLUE], NULL),
				g_key_file_get_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_ALPHA], NULL));

 	//! The Font is a Pango structure. This only can be added to a terminal
 	//! So we have to convert it.
// 	g_clear_pointer (term_conf->font, pango_font_description_free);
 	key_str = g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FONT], NULL);
 	term_conf->font = pango_font_description_from_string (key_str);
 	g_free (key_str);

	return term_conf;
}

//! Load the configuration from <section> into the serial config
//! If it does not exists it creates one from the defaults
static port_config_t *gtkterm_configuration_load_serial_config (GtkTermConfiguration *self, gpointer data, gpointer user_data) {
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);
	port_config_t *port_conf;
	char *section = (char *)data;
	char *key_str = NULL;	

	if (check_keyfile (self, section))
		return NULL;

	port_conf = g_malloc0 (sizeof (port_config_t));		

	port_conf->port = g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_PORT], NULL);
	port_conf->baudrate = g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BAUDRATE], NULL);
	port_conf->bits = g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BITS], NULL);
	port_conf->stopbits = g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_STOPBITS], NULL);
	port_conf->rs485_rts_time_before_transmit = g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX], NULL);
	port_conf->rs485_rts_time_after_transmit = g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX], NULL);
 	
	key_str = g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_PARITY], NULL);
 	if (key_str != NULL) {
 		if(!g_ascii_strcasecmp(key_str, "none"))
 			port_conf->parity = 0;
 		else if(!g_ascii_strcasecmp(key_str, "odd"))
 			port_conf->parity = 1;
 		else if(!g_ascii_strcasecmp(key_str, "even"))
 			port_conf->parity = 2;
 		g_free (key_str);
 	}

 	key_str = g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_FLOW_CONTROL], NULL);
 	if (key_str != NULL) {
 		if (!g_ascii_strcasecmp (key_str, "none"))
 			port_conf->flow_control = 0;
 		else if (!g_ascii_strcasecmp (key_str, "xon"))
 			port_conf->flow_control = 1;
 		else if (!g_ascii_strcasecmp (key_str, "rts"))
 			port_conf->flow_control = 2;
 		else if (!g_ascii_strcasecmp (key_str, "rs485"))
 			port_conf->flow_control = 3;
    		
 		g_free (key_str);
 	}
 
	key_str = g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_DISABLE_PORT_LOCK], NULL);
 	if (key_str != NULL) {
 		port_conf->disable_port_lock = g_ascii_strcasecmp (key_str, "true") ? true : false;

		g_free (key_str);
 	}

	return port_conf;
}

//! @brief
//! Create a new <default> configuration
void gtkterm_configuration_default_configuration (GtkTermConfigurationPrivate *priv, char *section) {

	g_key_file_set_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_PORT], DEFAULT_PORT);
	g_key_file_set_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BAUDRATE], DEFAULT_BAUDRATE);
	g_key_file_set_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_PARITY], DEFAULT_PARITY);
	g_key_file_set_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BITS], DEFAULT_BITS);
	g_key_file_set_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_STOPBITS], DEFAULT_STOPBITS);
	g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_FLOW_CONTROL], DEFAULT_FLOW);
	g_key_file_set_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX], DEFAULT_DELAY_RS485);
	g_key_file_set_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX], DEFAULT_DELAY_RS485);
	g_key_file_set_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_DISABLE_PORT_LOCK], "false");

	g_key_file_set_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_CHAR], DEFAULT_CHAR);
	g_key_file_set_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_DELAY],DEFAULT_DELAY);
	g_key_file_set_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_ECHO], DEFAULT_ECHO);
	g_key_file_set_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_CRLF_AUTO], "false");
	g_key_file_set_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FONT], DEFAULT_FONT);
	g_key_file_set_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BLOCK_CURSOR], "true");
	g_key_file_set_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_SHOW_CURSOR], "true");
	g_key_file_set_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_ROWS], 80);
	g_key_file_set_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_COLS], 25);
	g_key_file_set_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_SCROLLBACK], DEFAULT_SCROLLBACK);
	g_key_file_set_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_VISUAL_BELL], DEFAULT_VISUAL_BELL);

 	g_key_file_set_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_RED], 0.66);
 	g_key_file_set_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_GREEN], 0.66);
 	g_key_file_set_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_BLUE], 0.66);
 	g_key_file_set_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_ALPHA], 1);

 	g_key_file_set_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_RED], 0);
 	g_key_file_set_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_GREEN], 0);
 	g_key_file_set_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_BLUE], 0);
 	g_key_file_set_double (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_ALPHA], 1);

	g_key_file_set_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_MACROS], "");
}

//! @brief
//! validate the configuration, given by the section
void gtkterm_configuration_validate(GtkTermConfigurationPrivate *priv, char *section)
{
 	char *string = NULL;
	int value;
	unsigned long lvalue;

	lvalue = g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BAUDRATE], NULL);
	switch (lvalue) 	{
		case 300:
		case 600:
		case 1200:
		case 2400:
		case 4800:
		case 9600:
		case 19200:
		case 38400:
		case 57600:
		case 115200:
		case 230400:
		case 460800:
		case 576000:
		case 921600:
		case 1000000:
		case 2000000:
			break;

		default:
			string = g_strdup_printf(_("Baudrate %ld may not be supported by all hardware"), lvalue);
			show_message(string, MSG_ERR);
	    
			g_free(string);
	}

	value = g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_STOPBITS], NULL);
	if(value  != 1 && value  != 2)
	{
		string = g_strdup_printf(_("Invalid number of stop-bits: %d\nFalling back to default number of stop-bits number: %d\n"), value, DEFAULT_STOPBITS);
		show_message(string, MSG_ERR);
		g_key_file_set_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_STOPBITS], DEFAULT_STOPBITS);

		g_free(string);
	}

	value = g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BITS], NULL);
	if(value < 5 || value> 8)
	{
		string = g_strdup_printf(_("Invalid number of bits: %d\nFalling back to default number of bits: %d\n"), value, DEFAULT_BITS);
		show_message(string, MSG_ERR);
		g_key_file_set_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BITS], DEFAULT_BITS);

		g_free(string);
	}

	value = g_key_file_get_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_DELAY], NULL);
	if(value < 0 || value> 500)
	{
		string = g_strdup_printf(_("Invalid delay: %d ms\nFalling back to default delay: %d ms\n"), value, DEFAULT_DELAY);
		show_message(string, MSG_ERR);
		g_key_file_set_integer (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_DELAY],DEFAULT_DELAY);

		g_free(string);
	}

	if(g_key_file_get_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FONT], NULL) == NULL)
		g_key_file_set_string (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FONT], DEFAULT_FONT);
}

//! @brief
//! Set the config option in the keyfile.
//! Options are not saved.
bool on_set_config_options (const char *name, const char *value, gpointer data,  GError **error) {

	int item_counter = 0;
	int config_option_success = true;
	char *section = GTKTERM_APP(data)->section;
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(GTKTERM_APP(data)->config);

	//! Point to the third charater ('--' in front of the cli option)
	name += 2;

	//! Search index for the option we want to set
	while (item_counter < CONF_ITEM_LAST) {
		if (!g_strcmp0 (name, GtkTermConfigurationItems[item_counter]))
			break;

		item_counter++;
	}

	switch (item_counter) {
		case CONF_ITEM_TERM_ECHO:
		case CONF_ITEM_SERIAL_DISABLE_PORT_LOCK:
		case CONF_ITEM_SERIAL_PARITY:
		case CONF_ITEM_SERIAL_FLOW_CONTROL:		
			g_key_file_set_string (priv->key_file, section, GtkTermConfigurationItems[item_counter], value);		
			break;

		case CONF_ITEM_SERIAL_BAUDRATE:
			g_key_file_set_integer (priv->key_file, section, GtkTermConfigurationItems[item_counter], atol(value));	
			break;


		case CONF_ITEM_SERIAL_BITS:
		case CONF_ITEM_SERIAL_STOPBITS:
		case CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX:
		case CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX:
		case CONF_ITEM_TERM_WAIT_CHAR:
		case CONF_ITEM_TERM_WAIT_DELAY:		
			g_key_file_set_integer (priv->key_file, section, GtkTermConfigurationItems[item_counter], atoi(value));	
			break;

		case CONF_ITEM_TERM_RAW_FILENAME:
		case CONF_ITEM_SERIAL_PORT:
    		//! Check for max path length. Exit if it is to long.
			//! Note: Serial port is also a path to a device.
    		if ((int)strlen(value) < PATH_MAX) {
        		g_key_file_set_string (priv->key_file, section, GtkTermConfigurationItems[item_counter], value);

    		} else {
        		g_printf (_("Filename to long\n\n"));

        		config_option_success = false;
			}
	
			break;

		default:
				//! We should not get here.
				g_printf ("Invalid option. Internal lookup failure");
				config_option_success = false;
				break;
	}

	g_free (section);

	if (config_option_success)
		gtkterm_configuration_validate (priv, section);

	return config_option_success;
}

//! Convert the colors RGB to internal color scheme
static void set_color(GdkRGBA *color, float R, float G, float B, float A) {
	color->red = R;
	color->green = G;
	color->blue = B;
 	color->alpha = A;
}


// int load_configuration_from_file(char *section)
// {

// 	//! First initialize with a default structure.
// 	//! Not really needed but good practice.
// 	hard_default_configuration();



// 	/// Convert the stringlist to macros. Existing shortcuts will be delete from convert_string_to_macros
// 	macrostring = g_key_file_get_string_list (config_object, section, GtkTermConfigurationItems[CONF_ITEM_MACROS], &nr_of_strings, NULL);
// 	convert_string_to_macros (macrostring, nr_of_strings);
// 	g_strfreev(macrostring);

// 	return 0;
// }

// //! This checks if the configuration file exists. If not it creates a
// //! new [default]
// int check_configuration_file(void)
// {
// 	struct stat my_stat;
// 	char *string = NULL;

// 	/* is configuration file present ? */
// 	if(stat(g_file_get_path(config_file), &my_stat) == 0)
// 	{
// 		/* If bad configuration file, fallback to _hardcoded_ defaults! */
// 		if(load_configuration_from_file(DEFAULT_SECTION) == -1)
// 		{
// 			hard_default_configuration();
// 			return -1;
// 		}
// 	} /* if not, create it, with the [default] section */
// 	else
// 	{
// 		GKeyFile *config;

// 		string = g_strdup_printf(_("Configuration file (%s) with [default] configuration has been created.\n"), g_file_get_path(config_file));
// 		show_message(string, MSG_WRN);
// 		g_free(string);

// 		config = g_key_file_new ();
// 		hard_default_configuration();

// 		//! Put the new default in the key file
// 		copy_configuration(config, DEFAULT_SECTION);

// 		//! And save the config to file
// 		save_configuration_to_file(config);		

// 		string = g_strdup_printf(_("Configuration [%s] saved\n"), term_conf.active_section);
// 		show_message(string, MSG_WRN);
// 		g_free(string);

// 		g_key_file_unref (config);
// 	}

// 	return 0;
// }

// //! Copy the active configuration into <section> of the Key file
// void copy_configuration(GKeyFile *configrc, const char *section)
// {
// 	char *string = NULL;
// 	char **string_list = NULL;
// 	gsize nr_of_strings = 0;

// 	g_key_file_set_string (configrc, section, GtkTermConfigurationItems[CONF_ITEM_PORT], port_conf.port);
// 	g_key_file_set_integer (configrc, section, GtkTermConfigurationItems[CONF_ITEM_BAUDRATE], port_conf.baudrate);
// 	g_key_file_set_integer (configrc, section, GtkTermConfigurationItems[CONF_ITEM_BITS], port_conf.bits);
// 	g_key_file_set_integer (configrc, section, GtkTermConfigurationItems[CONF_ITEM_STOPBITS], port_conf.stopbits);

// 	switch(port_conf.parity)
// 	{
// 		case 0:
// 			string = g_strdup_printf("none");
// 			break;
// 		case 1:
// 			string = g_strdup_printf("odd");
// 			break;
// 		case 2:
// 			string = g_strdup_printf("even");
// 			break;
// 		default:
// 		    string = g_strdup_printf("none");
// 	}

// 	g_key_file_set_string (configrc, section, GtkTermConfigurationItems[CONF_ITEM_PARITY], string);
// 	g_free(string);

// 	switch(port_conf.flow_control)
// 	{
// 		case 0:
// 			string = g_strdup_printf("none");
// 			break;
// 		case 1:
// 			string = g_strdup_printf("xon");
// 			break;
// 		case 2:
// 			string = g_strdup_printf("rts");
// 			break;
// 		case 3:
// 			string = g_strdup_printf("rs485");
// 			break;
// 		default:
// 			string = g_strdup_printf("none");
// 	}

// 	g_key_file_set_string (configrc, section, GtkTermConfigurationItems[CONF_ITEM_FLOW_CONTROL], string);
// 	g_free(string);

// 	g_key_file_set_integer (configrc, section, GtkTermConfigurationItems[CONF_ITEM_WAIT_DELAY], term_conf.delay);
// 	g_key_file_set_integer (configrc, section, GtkTermConfigurationItems[CONF_ITEM_WAIT_CHAR], term_conf.char_queue);
// 	g_key_file_set_integer (configrc, section, GtkTermConfigurationItems[CONF_ITEM_RS485_RTS_TIME_BEFORE_TX],
//     	                        port_conf.rs485_rts_time_before_transmit);
// 	g_key_file_set_integer (configrc, section, GtkTermConfigurationItems[CONF_ITEM_RS485_RTS_TIME_AFTER_TX],
//     	                        port_conf.rs485_rts_time_after_transmit);

// 	g_key_file_set_boolean (configrc, section, GtkTermConfigurationItems[CONF_ITEM_ECHO], term_conf.echo);
// 	g_key_file_set_boolean (configrc, section, GtkTermConfigurationItems[CONF_ITEM_CRLF_AUTO], term_conf.crlfauto);
// 	g_key_file_set_boolean (configrc, section, GtkTermConfigurationItems[CONF_ITEM_DISABLE_PORT_LOCK], port_conf.disable_port_lock);

// 	string = pango_font_description_to_string (term_conf.font);
// 	g_key_file_set_string (configrc, section, GtkTermConfigurationItems[CONF_ITEM_FONT], string);
// 	g_free(string);

// 	//! Macros are an array of strings, so we have to convert it
// 	//! All macros ends up in the string_list
// 	string_list = g_malloc ( macro_count () * sizeof (char *) * 2 + 1);
// 	nr_of_strings = convert_macros_to_string (string_list);
// 	g_key_file_set_string_list (configrc, section, GtkTermConfigurationItems[CONF_ITEM_MACROS], (const char * const*) string_list, nr_of_strings);
// 	g_free(string_list);	

// 	g_key_file_set_boolean (configrc, section, GtkTermConfigurationItems[CONF_ITEM_TERM_SHOW_CURSOR], term_conf.show_cursor);
// 	g_key_file_set_boolean (configrc, section, GtkTermConfigurationItems[CONF_ITEM_TERM_TIMESTAMP], term_conf.timestamp);
// 	g_key_file_set_boolean (configrc, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BLOCK_CURSOR], term_conf.block_cursor);
// 	g_key_file_set_integer (configrc, section, GtkTermConfigurationItems[CONF_ITEM_TERM_ROWS], term_conf.rows);
// 	g_key_file_set_integer (configrc, section, GtkTermConfigurationItems[CONF_ITEM_TERM_COLS], term_conf.columns);
// 	g_key_file_set_integer (configrc, section, GtkTermConfigurationItems[CONF_ITEM_TERM_SCROLLBACK], term_conf.scrollback);
// 	g_key_file_set_boolean (configrc, section, GtkTermConfigurationItems[CONF_ITEM_TERM_VISUAL_BELL], term_conf.visual_bell);

// 	g_key_file_set_double (configrc, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_RED], term_conf.foreground_color.red);
// 	g_key_file_set_double (configrc, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_GREEN], term_conf.foreground_color.green);
// 	g_key_file_set_double (configrc, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_BLUE], term_conf.foreground_color.blue);
// 	g_key_file_set_double (configrc, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_ALPHA], term_conf.foreground_color.alpha);

// 	g_key_file_set_double (configrc, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_RED], term_conf.background_color.red);
// 	g_key_file_set_double (configrc, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_GREEN], term_conf.background_color.green);
// 	g_key_file_set_double (configrc, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_BLUE], term_conf.background_color.blue);
// 	g_key_file_set_double (configrc, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_ALPHA], term_conf.background_color.alpha);
// }
