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

#include "config.h"
#include "gtkterm_defaults.h"
#include "gtkterm_window.h"
#include "gtkterm_serial_port.h"
#include "gtkterm_terminal.h"
#include "gtkterm_configuration.h"
#include "macros.h"

/**
 * @brief Configuration options
 *
 * Used configuration options to hold consistency between load/save functions
 * Also used as long-option when configuring by CLI
 *
 * @todo Add the short option.
 *
 */
const char GtkTermConfigurationItems[][CONF_ITEM_LENGTH] = {
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

const char GtkTermCLIShortOption[][CONF_ITEM_LENGTH] = {
	"p",
	"s",
	"b",
	"t",
	"a",
	"w",
	"d",
	"r",
	"x",
	"y",
	"",
	"f",
	"e",
	"",
	"l",
	"",
	"",
	"",
	"",
	"o",
	"c",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
};

G_BEGIN_DECLS

typedef struct
{
	GKeyFile *key_file;	 						/**< The memory loaded keyfile				*/
	GFile *config_file; 						/**< The config file						*/

	GError *config_error;						/**< Error of the last file operation		*/
	GtkTermConfigurationState config_status; 	/**< Status when operating configfiles		*/

	bool config_is_dirty;						/**< If changes are made but not yet saved	*/

} GtkTermConfigurationPrivate;

struct _GtkTermConfiguration
{

	GObject parent_instance;
};

struct _GtkTermConfigurationClass
{

	GObjectClass parent_class;
};

G_DEFINE_TYPE_WITH_PRIVATE(GtkTermConfiguration, gtkterm_configuration, G_TYPE_OBJECT)

/**
 * @brief Callback functions for signals
 */
static int gtkterm_configuration_load_keyfile(GtkTermConfiguration *, gpointer);
static int gtkterm_configuration_save_keyfile(GtkTermConfiguration *, gpointer);
static int gtkterm_configuration_list_config(GtkTermConfiguration *, gpointer);
static int gtkterm_configuration_print_section(GtkTermConfiguration *, gpointer, gpointer);
static int gtkterm_configuration_remove_section(GtkTermConfiguration *, gpointer, gpointer);
static int gtkterm_configuration_set_config_file(GtkTermConfiguration *, gpointer);
static term_config_t *gtkterm_configuration_load_terminal_config(GtkTermConfiguration *, gpointer, gpointer);
static port_config_t *gtkterm_configuration_load_serial_config(GtkTermConfiguration *, gpointer, gpointer);
static int gtkterm_configuration_copy_section(GtkTermConfiguration *, gpointer, gpointer, gpointer, gpointer);

/**
 * @brief Functions for internal use only
 *
 */
static void set_color(GdkRGBA *, float, float, float, float);

static GtkTermConfigurationState gtkterm_configuration_check_configuration_file(GtkTermConfiguration *);
void gtkterm_configuration_default_configuration(GtkTermConfiguration *, char *);
GtkTermConfigurationState gtkterm_configuration_validate(GtkTermConfiguration *, char *);
GtkTermConfigurationState gtkterm_configuration_set_status(GtkTermConfiguration *self, GtkTermConfigurationState, GError *);

/**
 * @brief Remote all pointers when removing the object.
 * 
 * @param object The pointer to the configuration object.
 */
static void gtkterm_configuration_finalize (GObject *object) {
   	GtkTermConfiguration *self = GTKTERM_CONFIGURATION(object);
    GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private (self);

    g_clear_error (&priv->config_error);

    GObjectClass *object_class = G_OBJECT_CLASS (gtkterm_configuration_parent_class);
    object_class->finalize (object);	
}

/**
 * @brief  Connect callback functions to signals.
 *
 * The callback functions performs the operation on the keyfile or configuration.
 *
 * @param object The configuration class object.
 *
 */
static void gtkterm_configuration_class_constructed(GObject *object) {
	GtkTermConfiguration *self = GTKTERM_CONFIGURATION(object);

	g_signal_connect(self, "config_print", G_CALLBACK(gtkterm_configuration_print_section), NULL);
	g_signal_connect(self, "config_remove", G_CALLBACK(gtkterm_configuration_remove_section), NULL);
	g_signal_connect(self, "config_load", G_CALLBACK(gtkterm_configuration_load_keyfile), NULL);
	g_signal_connect(self, "config_save", G_CALLBACK(gtkterm_configuration_save_keyfile), NULL);
	g_signal_connect(self, "config_list", G_CALLBACK(gtkterm_configuration_list_config), NULL);	
	g_signal_connect(self, "config_terminal", G_CALLBACK(gtkterm_configuration_load_terminal_config), NULL);
	g_signal_connect(self, "config_serial", G_CALLBACK(gtkterm_configuration_load_serial_config), NULL);
	g_signal_connect(self, "config_copy", G_CALLBACK(gtkterm_configuration_copy_section), NULL);
	g_signal_connect(self, "config_check", G_CALLBACK(gtkterm_configuration_set_config_file), NULL);	

	G_OBJECT_CLASS(gtkterm_configuration_parent_class)->constructed(object);
}

/**
 * @brief  Initialize the class functions.
 *
 * Nothing special to do here.
 *
 * @param class The configuration.
 *
 */
static void gtkterm_configuration_class_init(GtkTermConfigurationClass *class) {
	GObjectClass *object_class = G_OBJECT_CLASS(class);

	object_class->constructed = gtkterm_configuration_class_constructed;
	object_class->finalize = gtkterm_configuration_finalize;	
}

/**
 * @brief  Initialize the class members.
 *
 * @param self  the configuration.
 *
 */
static void gtkterm_configuration_init(GtkTermConfiguration *self) {

	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);

	//! Initialize to NULL so we can detect if it is loaded.
	priv->config_file = NULL;
	priv->key_file = NULL;
	priv->config_error = NULL;
	priv->config_is_dirty = false;
}

/**
 * @brief Check if the configuration file exists on disk.
 *
 * If not it creates and new default one and save it to disk.
 *
 * @param self The configuration class.
 *
 * @return The result of the operation
 *
 */
static GtkTermConfigurationState gtkterm_configuration_check_configuration_file(GtkTermConfiguration *self)
{
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);
	GtkTermConfigurationState rc = GTKTERM_CONFIGURATION_SUCCESS;
	GError *error = NULL;
	struct stat my_stat;

	/** is configuration file present
	 * if not, create it, with the [default] section
	 */
	if (stat(g_file_get_path(priv->config_file), &my_stat) != 0) 	{

		priv->key_file = g_key_file_new();
		gtkterm_configuration_default_configuration(self, DEFAULT_SECTION);

		/** And save the keyfile */
		gtkterm_configuration_save_keyfile(self, NULL);

		error = g_error_new (g_quark_from_static_string ("GTKTERM_CONFIGURATION"),
                             GTKTERM_CONFIGURATION_FILE_CREATED,
                             _("Configuration file with [default] configuration has been created and saved")
                             );

		rc = GTKTERM_CONFIGURATION_FILE_CREATED;
	}

	return gtkterm_configuration_set_status(self, rc, error);
}

/**
 * @brief Check if the file exists in the old/new location
 *
 * Old location of configuration file was $HOME/.gtktermrc.
 * New location is $XDG_CONFIG_HOME/.gtktermrc.
 * If configuration file exists at new location, use that one.
 * Otherwise, if file exists at old location, move file to new location.
 *
 * Version 2.0: Because we have to use gtkterm_conv, the file is always at
 * the user directory. So we can skip eventually moving the file.
 *
 * @param self The configuration class.
 * 
 * @param user_data Not used.
 *
 * @return The result of the operation.
 */
static int gtkterm_configuration_set_config_file (GtkTermConfiguration *self, gpointer user_data) {
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);

	GFile *config_file_old = g_file_new_build_filename(getenv("HOME"), CONFIGURATION_FILENAME, NULL);
	priv->config_file = g_file_new_build_filename(g_get_user_config_dir(), CONFIGURATION_FILENAME, NULL);

	if (!g_file_query_exists(priv->config_file, NULL) && g_file_query_exists(config_file_old, NULL))
		g_file_move(config_file_old, priv->config_file, G_FILE_COPY_NONE, NULL, NULL, NULL, NULL);

	// Check it the config file exists and if not, create one.
	return gtkterm_configuration_check_configuration_file(self);
}

/**
 * @brief  Check if the keyfile is loaded into memory.
 *
 * Loads the keyfile and checks if the section we want to access, exists.
 *
 * @param self The configuration for which the get the status for.
 *
 * @param section The section we want the configuration to read from
 *
 * @return: The status of this operation
 *
 */
GtkTermConfigurationState check_keyfile(GtkTermConfiguration *self, char *section) {
	GError *error = NULL;
	GtkTermConfigurationState rc = GTKTERM_CONFIGURATION_SUCCESS;
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);

	/** Load keyfile if it is nog loaded yet */
	if (priv->key_file == NULL)	{

		rc = gtkterm_configuration_load_keyfile(self, NULL);

		if (!(rc == GTKTERM_CONFIGURATION_SUCCESS || rc == GTKTERM_CONFIGURATION_FILE_CREATED))
			return rc;	
	}

	/** Check if the [section] exists in the key file. */
	if (!g_key_file_has_group(priv->key_file, section))	 {
        
		error = g_error_new (g_quark_from_static_string ("GTKTERM_CONFIGURATION"),
                             GTKTERM_CONFIGURATION_SECTION_UNKNOWN,
                             _("No [%s] section in configuration file. Section is created."),
                             section);

		/** we did not find the section so we create it */
		gtkterm_configuration_default_configuration(self, section);	

		/** Set the dirty flag */
		priv->config_is_dirty = true;						 

		rc = GTKTERM_CONFIGURATION_SUCCESS;
	}

	return gtkterm_configuration_set_status (self, rc, error);
}

/**
 * @brief Lists all sections in the keyfile.
 *
 * @param self The configuration class.
 *
 * @param user_data Not used.
 *
 * @return The result of the operation.
 */
static int gtkterm_configuration_list_config (GtkTermConfiguration *self, gpointer user_data) {
	size_t nr_of_groups;
	char **sections;
	GtkTermConfigurationState rc = GTKTERM_CONFIGURATION_SUCCESS;
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);

	if ((rc = gtkterm_configuration_load_keyfile(self, user_data)) != GTKTERM_CONFIGURATION_SUCCESS)
		return rc;

	sections = g_key_file_get_groups (priv->key_file, &nr_of_groups);

	if (sections == NULL)
		return GTKTERM_CONFIGURATION_SUCCESS;

	g_printf ("Configurations found in keyfile:\n\n");
	for (int i = 0; i < nr_of_groups; i++)
		g_printf ("[%s]\n", sections[i]);
	g_printf ("\n");
	
	return gtkterm_configuration_set_status(self, rc, NULL);
}

/**
 * @brief Load the key file into memory.
 *
 * The keyfile with all sections are loaded into memory.
 * It is in raw format. All on/off etc are not translated yet.
 *
 * @param self The configuration class.
 *
 * @param user_data Not used.
 *
 * @return The result of the operation.
 */
static int gtkterm_configuration_load_keyfile (GtkTermConfiguration *self, gpointer user_data) {
	GError *error = NULL;
	GtkTermConfigurationState rc = GTKTERM_CONFIGURATION_SUCCESS;
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);

	if (priv->config_file == NULL) {
		rc = gtkterm_configuration_set_config_file (self, NULL);

		if (!(rc == GTKTERM_CONFIGURATION_SUCCESS || rc == GTKTERM_CONFIGURATION_FILE_CREATED))
			return rc;			
	}

	priv->key_file = g_key_file_new();

	if (!g_key_file_load_from_file(priv->key_file, g_file_get_path(priv->config_file), G_KEY_FILE_NONE, &error)) {

		g_propagate_error (&error, 
							g_error_new (g_quark_from_static_string ("GTKTERM_CONFIGURATION"),
							GTKTERM_CONFIGURATION_FILE_CONFIG_LOAD,
							_("Failed to load configuration file: %s\n%s"),
							g_file_get_path(priv->config_file),
							error->message));

		rc = GTKTERM_CONFIGURATION_FILE_CONFIG_LOAD;
	}

	return gtkterm_configuration_set_status(self, rc, error);
}

/**
 * @brief Save the in momeory keyfile to file).
 *
 * The keyfile with all sections saved to file
 *
 * @param self The configuration class.
 *
 * @param user_data Not used.
 *
 * @return The result of the operation.
 */
static int gtkterm_configuration_save_keyfile(GtkTermConfiguration *self, gpointer user_data)
{
	GError *error = NULL;
	GtkTermConfigurationState rc = GTKTERM_CONFIGURATION_SUCCESS;	
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);

	if (priv->config_file == NULL) 	{
		error = g_error_new (g_quark_from_static_string ("GTKTERM_CONFIGURATION"),
                             GTKTERM_CONFIGURATION_NO_KEYFILE_LOADED,
                             _ ("File not saved. No keyfile loaded")
                             );

		rc = GTKTERM_CONFIGURATION_NO_KEYFILE_LOADED;

	} else  if (!g_key_file_save_to_file(priv->key_file, g_file_get_path(priv->config_file), &error)){
		
		g_propagate_error (&error, 
							g_error_new (g_quark_from_static_string ("GTKTERM_CONFIGURATION"),
                          	GTKTERM_CONFIGURATION_FILE_NOT_SAVED,
                          	_("Failed to save configuration file: %s\n%s"),
                          	g_file_get_path(priv->config_file),
						 	error->message));

		rc = GTKTERM_CONFIGURATION_FILE_NOT_SAVED;
	} else {
		rc = GTKTERM_CONFIGURATION_FILE_SAVED;

		error = g_error_new (g_quark_from_static_string ("GTKTERM_CONFIGURATION"),
                             GTKTERM_CONFIGURATION_FILE_SAVED,
                             _("Configuration saved")
                             );

		/** Reset the dirty flag now we saved the keyfile */
		priv->config_is_dirty = false;							 
	}

	return gtkterm_configuration_set_status(self, rc, error);
}

/**
 * @brief Print the section to CLI.
 *
 * @param self The configuration class.
 *
 * @param data Pointer to the section we want to show
 *
 * @param user_data Not used.
 *
 * @return The result of the operation.
 */
static int gtkterm_configuration_print_section(GtkTermConfiguration *self, gpointer data, gpointer user_data)
{
	char *section = (char *)data;
	GtkTermConfigurationState rc;
	gsize nr_of_strings;
	char **macrostring;
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);

	if ((rc = check_keyfile(self, section) != GTKTERM_CONFIGURATION_SUCCESS))
		return rc;

	g_printf(_("Configuration [%s] loaded from file\n"), section);

	/** Print the serial port items */
	g_printf(_("\nSerial port settings:\n"));
	g_printf(_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_PORT], g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_PORT], NULL));
	g_printf(_("%-24s : %ld\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_BAUDRATE], (unsigned long)g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BAUDRATE], NULL));
	g_printf(_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_BITS], g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BITS], NULL));
	g_printf(_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_STOPBITS], g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_STOPBITS], NULL));
	g_printf(_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_PARITY], g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_PARITY], NULL));
	g_printf(_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_FLOW_CONTROL], g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_FLOW_CONTROL], NULL));
	g_printf(_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX], g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX], NULL));
	g_printf(_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX], g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX], NULL));
	g_printf(_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_SERIAL_DISABLE_PORT_LOCK], g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_DISABLE_PORT_LOCK], NULL));

	/** Print the terminal items */
	g_printf(_("\nTerminal settings:\n"));
	g_printf(_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_FONT], g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FONT], NULL));
	g_printf(_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_RAW_FILENAME], g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_RAW_FILENAME], NULL));
	g_printf(_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_ECHO], g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_ECHO], NULL));
	g_printf(_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_CRLF_AUTO], g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_CRLF_AUTO], NULL));
	g_printf(_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_DELAY], g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_DELAY], NULL));
	g_printf(_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_CHAR], g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_CHAR], NULL));
	g_printf(_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_TIMESTAMP], g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_TIMESTAMP], NULL));
	g_printf(_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_BLOCK_CURSOR], g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BLOCK_CURSOR], NULL));
	g_printf(_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_SHOW_CURSOR], g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_SHOW_CURSOR], NULL));
	g_printf(_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_ROWS], g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_ROWS], NULL));
	g_printf(_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_COLS], g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_COLS], NULL));
	g_printf(_("%-24s : %d\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_SCROLLBACK], g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_SCROLLBACK], NULL));
	g_printf(_("%-24s : %s\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_VISUAL_BELL], g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_VISUAL_BELL], NULL));
	g_printf(_("%-24s : %f\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_RED], g_key_file_get_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_RED], NULL));
	g_printf(_("%-24s : %f\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_BLUE], g_key_file_get_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_BLUE], NULL));
	g_printf(_("%-24s : %f\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_GREEN], g_key_file_get_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_GREEN], NULL));
	g_printf(_("%-24s : %f\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_ALPHA], g_key_file_get_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_ALPHA], NULL));
	g_printf(_("%-24s : %f\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_RED], g_key_file_get_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_RED], NULL));
	g_printf(_("%-24s : %f\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_BLUE], g_key_file_get_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_BLUE], NULL));
	g_printf(_("%-24s : %f\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_GREEN], g_key_file_get_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_GREEN], NULL));
	g_printf(_("%-24s : %f\n"), GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_ALPHA], g_key_file_get_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_ALPHA], NULL));

	/** Convert the stringlist to macros. Existing shortcuts will be deleted from convert_string_to_macros */
	macrostring = g_key_file_get_string_list(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_MACROS], &nr_of_strings, NULL);
	convert_string_to_macros(macrostring, nr_of_strings);
	g_strfreev(macrostring);

	/** ... and the macro's */
	g_printf(_("\nMacros:\n"));
	g_printf(_(" Nr  Shortcut  Command\n"));

	if (macro_count() == 0)
		g_printf(_("No macros defined in this section\n"));
	else
	{
		for (int i = 0; i < macro_count(); i++)
			g_printf("[%2d] %-8s  %s\n", i, macros[i].shortcut, macros[i].action);
	}

	if ((rc = gtkterm_configuration_validate(self, section)) != GTKTERM_CONFIGURATION_SUCCESS)
		return rc;

	/** Inverse the return due to the handling return value of the callback */
	return gtkterm_configuration_set_status(self, rc, NULL);
}

/**
 * @brief Remove a section from the GKeyFile.
 *
 * If it is the active section then switch back to default.
 * If it is the default section then create a new 'default' default section
 *
 * @param self The configuration class.
 *
 * @param data Pointer to the section we want to remove.
 *
 * @param user_data Not used.
 *
 * @return The result of the operation.
 */
static int gtkterm_configuration_remove_section(GtkTermConfiguration *self, gpointer data, gpointer user_data)
{
	GtkTermConfigurationState rc = GTKTERM_CONFIGURATION_SUCCESS;
	char *section = (char *)data;
	GError *error = NULL;
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);

	if ((rc = check_keyfile(self, section)) != GTKTERM_CONFIGURATION_SUCCESS)
		return rc;

	/** If we remove the DEFAULT_SECTION then create a new one */
	if (!g_strcmp0((const char *)section, (const char *)&DEFAULT_SECTION)) {

		gtkterm_configuration_default_configuration(self, section);
	}
	else if (!g_key_file_remove_group(priv->key_file, section, &error))	{
		
		/** Remove the group from GKeyFile */
		g_propagate_error (&error, 
							g_error_new (g_quark_from_static_string ("GTKTERM_CONFIGURATION"),
                          	GTKTERM_CONFIGURATION_SECTION_NOT_REMOVED,
                          	_("Failed to remove section: %s\n%s"),
                          	section,
						 	error->message));


			rc = GTKTERM_CONFIGURATION_SECTION_NOT_REMOVED;
	} else {
		gtkterm_configuration_save_keyfile(self, user_data);

		rc = GTKTERM_CONFIGURATION_SECTION_REMOVED;

		error = g_error_new (g_quark_from_static_string ("GTKTERM_CONFIGURATION"),
                             GTKTERM_CONFIGURATION_SECTION_REMOVED,
                             _ ("Section [%s] removed"),
							 section
                             );
	}

	return gtkterm_configuration_set_status(self, rc, error);
}

/**
 * @brief Copy the active configuration into [section] of the Key file.
 *
 * The pc and tc are the config items from a terminal window. They stay
 * in memory until an explicite save is given.
 *
 * @param self The configuration class.
 *
 * @param sect Section we want to copy the config into.
 *
 * @param pc The port configuration.
 *
 * @param tc Terminal configuration.
 *
 * @param user_data Not used.
 *
 * @return The result of the operation.
 */
static int gtkterm_configuration_copy_section(GtkTermConfiguration *self, gpointer sect, gpointer pc, gpointer tc, gpointer user_data)
{
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);
	port_config_t *port_conf = (port_config_t *)pc;
	term_config_t *term_conf = (term_config_t *)tc;
	char *section = (char *)sect;

	char *string = NULL;
	//	char **string_list = NULL;
	//	gsize nr_of_strings = 0;

	g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_PORT], port_conf->port);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BAUDRATE], port_conf->baudrate);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BITS], port_conf->bits);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_STOPBITS], port_conf->stopbits);

	switch (port_conf->parity) {

		case GTKTERM_SERIAL_PORT_PARITY_NONE:
			string = g_strdup_printf("none");
			break;
		case GTKTERM_SERIAL_PORT_PARITY_ODD:
			string = g_strdup_printf("odd");
			break;
		case GTKTERM_SERIAL_PORT_PARITY_EVEN:
			string = g_strdup_printf("even");
			break;
		default:
			string = g_strdup_printf("none");
	}

	g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_PARITY], string);
	g_free(string);

	switch (port_conf->flow_control) {

		case GTKTERM_SERIAL_PORT_FLOWCONTROL_NONE:
			string = g_strdup_printf("none");
			break;
		case GTKTERM_SERIAL_PORT_FLOWCONTROL_XON_XOFF:
			string = g_strdup_printf("xon");
			break;
		case GTKTERM_SERIAL_PORT_FLOWCONTROL_RTS_CTS:
			string = g_strdup_printf("rts");
			break;
		case GTKTERM_SERIAL_PORT_FLOWCONTROL_RS485_HD:
			string = g_strdup_printf("rs485");
			break;
		default:
			string = g_strdup_printf("none");
	}

	g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_FLOW_CONTROL], string);
	g_free(string);

	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_DELAY], term_conf->delay);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_CHAR], term_conf->char_queue);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX],
						   port_conf->rs485_rts_time_before_transmit);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX],
						   port_conf->rs485_rts_time_after_transmit);

	g_key_file_set_boolean(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_ECHO], term_conf->echo);
	g_key_file_set_boolean(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_CRLF_AUTO], term_conf->crlfauto);
	g_key_file_set_boolean(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_DISABLE_PORT_LOCK], port_conf->disable_port_lock);

	string = pango_font_description_to_string(term_conf->font);
	g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FONT], string);
	g_free(string);

	/** Macros are an array of strings, so we have to convert it
	 * All macros ends up in the string_list
	 */
	//	string_list = g_malloc ( macro_count () * sizeof (char *) * 2 + 1);
	//	nr_of_strings = convert_macros_to_string (string_list);
	//	g_key_file_set_string_list (priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_MACROS], (const char * const*) string_list, nr_of_strings);
	//	g_free(string_list);

	g_key_file_set_boolean(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_SHOW_CURSOR], term_conf->show_cursor);
	g_key_file_set_boolean(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_TIMESTAMP], term_conf->timestamp);
	g_key_file_set_boolean(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BLOCK_CURSOR], term_conf->block_cursor);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_ROWS], term_conf->rows);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_COLS], term_conf->columns);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_SCROLLBACK], term_conf->scrollback);
	g_key_file_set_boolean(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_VISUAL_BELL], term_conf->visual_bell);

	g_key_file_set_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_RED], term_conf->foreground_color.red);
	g_key_file_set_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_GREEN], term_conf->foreground_color.green);
	g_key_file_set_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_BLUE], term_conf->foreground_color.blue);
	g_key_file_set_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_ALPHA], term_conf->foreground_color.alpha);

	g_key_file_set_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_RED], term_conf->background_color.red);
	g_key_file_set_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_GREEN], term_conf->background_color.green);
	g_key_file_set_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_BLUE], term_conf->background_color.blue);
	g_key_file_set_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_ALPHA], term_conf->background_color.alpha);

	return gtkterm_configuration_set_status (self, GTKTERM_CONFIGURATION_SUCCESS, NULL);
}

/**
 * @brief Load the terminal configuration from keyfile
 *
 * Load the terminal configuration from [section] into the term config.
 * If it does not exists it creates one from the defaults
 *
 * @param self The configuration class.
 *
 * @param data The section we want to get the config from.
 *
 * @param user_data Not used.
 *
 * @return The terminal configuration which ends up as the last param in the signal. NULL on error.
 */
static term_config_t *gtkterm_configuration_load_terminal_config(GtkTermConfiguration *self, gpointer data, gpointer user_data) {
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);
	term_config_t *term_conf;
	char *section = (char *)data;
	char *key_str = NULL;
	int key_value = 0;

	if (check_keyfile(self, section) != GTKTERM_CONFIGURATION_SUCCESS)
		return NULL;

	term_conf = g_malloc0(sizeof(term_config_t));

	term_conf->delay = g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_DELAY], NULL);
	term_conf->rows = g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_ROWS], NULL);
	term_conf->columns = g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_COLS], NULL);
	term_conf->scrollback = g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_SCROLLBACK], NULL);

	key_str = g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BLOCK_CURSOR], NULL);
	if (key_str != NULL) {
		term_conf->block_cursor = g_ascii_strcasecmp(key_str, "true") ? true : false;

		g_free(key_str);
	}

	key_str = g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_SHOW_CURSOR], NULL);
	if (key_str != NULL) {
		term_conf->show_cursor = g_ascii_strcasecmp(key_str, "true") ? true : false;

		g_free(key_str);
	}

	key_str = g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_CRLF_AUTO], NULL);
	if (key_str != NULL) {
		term_conf->crlfauto = g_ascii_strcasecmp(key_str, "true") ? true : false;

		g_free(key_str);
	}

	key_str = g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_ECHO], NULL);
	if (key_str != NULL) {
		term_conf->echo = g_ascii_strcasecmp(key_str, "true") ? true : false;

		g_free(key_str);
	}

	key_str = g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_TIMESTAMP], NULL);
	if (key_str != NULL) {
		term_conf->timestamp = g_ascii_strcasecmp(key_str, "true") ? true : false;

		g_free(key_str);
	}

	key_str = g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_VISUAL_BELL], NULL);
	if (key_str != NULL) {
		term_conf->visual_bell = g_ascii_strcasecmp(key_str, "true") ? true : false;

		g_free(key_str);
	}

	key_value = g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_CHAR], NULL);
	term_conf->char_queue = key_value ? (signed char)key_value : -1;

	set_color(&term_conf->foreground_color,
			  g_key_file_get_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_RED], NULL),
			  g_key_file_get_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_GREEN], NULL),
			  g_key_file_get_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_BLUE], NULL),
			  g_key_file_get_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_ALPHA], NULL));

	set_color(&term_conf->background_color,
			  g_key_file_get_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_RED], NULL),
			  g_key_file_get_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_GREEN], NULL),
			  g_key_file_get_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_BLUE], NULL),
			  g_key_file_get_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_ALPHA], NULL));

	/** The Font is a Pango structure. This only can be added to a terminal
	 * So we have to convert it.
	 */
	// 	g_clear_pointer (term_conf->font, pango_font_description_free);
	key_str = g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FONT], NULL);
	term_conf->font = pango_font_description_from_string(key_str);
	g_free(key_str);

	return term_conf;
}

/**
 * @brief Load the portconfiguration from keyfile
 *
 * Load the port configuration from [section] into the term config.
 * If it does not exists it creates one from the defaults
 *
 * @param self The configuration class.
 *
 * @param data The section we want to get the config from.
 *
 * @param user_data Not used.
 *
 * @return The port configuration which ends up as the last param in the signal. NULL on error.
 */
static port_config_t *gtkterm_configuration_load_serial_config(GtkTermConfiguration *self, gpointer data, gpointer user_data) {
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);
	port_config_t *port_conf;
	char *section = (char *)data;
	char *key_str = NULL;

	if (check_keyfile(self, section) != GTKTERM_CONFIGURATION_SUCCESS)
		return NULL;

	port_conf = g_malloc0(sizeof(port_config_t));

	port_conf->port = g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_PORT], NULL);
	port_conf->baudrate = g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BAUDRATE], NULL);
	port_conf->bits = g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BITS], NULL);
	port_conf->stopbits = g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_STOPBITS], NULL);
	port_conf->rs485_rts_time_before_transmit = g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX], NULL);
	port_conf->rs485_rts_time_after_transmit = g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX], NULL);

	key_str = g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_PARITY], NULL);
	if (key_str != NULL) {
		if (!g_ascii_strcasecmp(key_str, "none"))
			port_conf->parity = GTKTERM_SERIAL_PORT_PARITY_NONE;
		else if (!g_ascii_strcasecmp(key_str, "odd"))
			port_conf->parity = GTKTERM_SERIAL_PORT_PARITY_ODD;
		else if (!g_ascii_strcasecmp(key_str, "even"))
			port_conf->parity = GTKTERM_SERIAL_PORT_PARITY_EVEN;
		g_free(key_str);
	}

	key_str = g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_FLOW_CONTROL], NULL);
	if (key_str != NULL) {
		if (!g_ascii_strcasecmp(key_str, "none"))
			port_conf->flow_control = GTKTERM_SERIAL_PORT_FLOWCONTROL_NONE;
		else if (!g_ascii_strcasecmp(key_str, "xon"))
			port_conf->flow_control = GTKTERM_SERIAL_PORT_FLOWCONTROL_XON_XOFF;
		else if (!g_ascii_strcasecmp(key_str, "rts"))
			port_conf->flow_control = GTKTERM_SERIAL_PORT_FLOWCONTROL_RTS_CTS;
		else if (!g_ascii_strcasecmp(key_str, "rs485"))
			port_conf->flow_control = GTKTERM_SERIAL_PORT_FLOWCONTROL_RS485_HD;

		g_free(key_str);
	}

	key_str = g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_DISABLE_PORT_LOCK], NULL);
	if (key_str != NULL) {
		port_conf->disable_port_lock = g_ascii_strcasecmp(key_str, "true") ? true : false;

		g_free(key_str);
	}

	return port_conf;
}

/**
 * @brief Create a new configuration with defaults.
 *
 * @param self The configuration class.
 *
 * @param section The section we want to get the config for.
 *
 */
void gtkterm_configuration_default_configuration(GtkTermConfiguration *self, char *section) {
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);

	g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_PORT], DEFAULT_PORT);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BAUDRATE], DEFAULT_BAUDRATE);
	g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_PARITY], DEFAULT_PARITY);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BITS], DEFAULT_BITS);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_STOPBITS], DEFAULT_STOPBITS);
	g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_FLOW_CONTROL], DEFAULT_FLOW);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX], DEFAULT_DELAY_RS485);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX], DEFAULT_DELAY_RS485);
	g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_DISABLE_PORT_LOCK], "false");

	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_CHAR], DEFAULT_CHAR);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_DELAY], DEFAULT_DELAY);
	g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_ECHO], DEFAULT_ECHO);
	g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_CRLF_AUTO], "false");
	g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FONT], DEFAULT_FONT);
	g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BLOCK_CURSOR], "true");
	g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_SHOW_CURSOR], "true");
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_ROWS], 80);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_COLS], 25);
	g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_SCROLLBACK], DEFAULT_SCROLLBACK);
	g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_VISUAL_BELL], DEFAULT_VISUAL_BELL);

	g_key_file_set_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_RED], 0.66);
	g_key_file_set_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_GREEN], 0.66);
	g_key_file_set_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_BLUE], 0.66);
	g_key_file_set_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FOREGROUND_ALPHA], 1);

	g_key_file_set_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_RED], 0);
	g_key_file_set_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_GREEN], 0);
	g_key_file_set_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_BLUE], 0);
	g_key_file_set_double(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_BACKGROUND_ALPHA], 1);

	g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_MACROS], "");
}

/**
 * @brief validate the configuration, given by the section.
 *
 * If not it creates and new default one and save it to disk.
 * When it finds an invalid config option it returns with an error
 * for which item the configuration check fails.
 *
 * @param self The configuration class.
 *
 * @param section The section we want to validate.
 *
 * @return The result of the operation
 *
 */
GtkTermConfigurationState gtkterm_configuration_validate(GtkTermConfiguration *self, char *section) {
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);
	GtkTermConfigurationState rc = GTKTERM_CONFIGURATION_SUCCESS;
	GError *error = NULL;
	int value;
	unsigned long lvalue;

	lvalue = g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BAUDRATE], NULL);
	switch (lvalue) {
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
				error = g_error_new (g_quark_from_static_string ("GTKTERM_CONFIGURATION"),
                             GTKTERM_CONFIGURATION_INVALID_BAUDRATE,
                             _ ("Baudrate %ld may not be supported by all hardware"),
							 lvalue
                             );

				rc =  GTKTERM_CONFIGURATION_INVALID_BAUDRATE;
				goto validate_exit;
	}

	value = g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_STOPBITS], NULL);
	if (value != 1 && value != 2) {
		
		error = g_error_new (g_quark_from_static_string ("GTKTERM_CONFIGURATION"),
                             GTKTERM_CONFIGURATION_INVALID_STOPBITS,
                             _ ("Invalid number of stopbits: %d\nFalling back to default number of stopbits: %d"),
							 value,
							 DEFAULT_STOPBITS
                             );

		rc = GTKTERM_CONFIGURATION_INVALID_STOPBITS;

		goto validate_exit;
	}

	value = g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_SERIAL_BITS], NULL);
	if (value < 5 || value > 8)	{
		
		error = g_error_new (g_quark_from_static_string ("GTKTERM_CONFIGURATION"),
                             GTKTERM_CONFIGURATION_INVALID_BITS,
                             _ ("Invalid number of bits: %d\nFalling back to default number of bits: %d"),
							 value,
							 DEFAULT_BITS
                             );

		rc = GTKTERM_CONFIGURATION_INVALID_BITS;

		goto validate_exit;		
	}

	value = g_key_file_get_integer(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_WAIT_DELAY], NULL);
	if (value < 0 || value > 500) {

		error = g_error_new (g_quark_from_static_string ("GTKTERM_CONFIGURATION"),
                             GTKTERM_CONFIGURATION_INVALID_DELAY,
                             _ ("Invalid delay: %d ms\nFalling back to default delay: %d ms"),
							 value,
							 DEFAULT_STOPBITS
                             );

		rc = GTKTERM_CONFIGURATION_INVALID_DELAY;

		goto validate_exit;
	}

	if (g_key_file_get_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FONT], NULL) == NULL)
		g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[CONF_ITEM_TERM_FONT], DEFAULT_FONT);

validate_exit:
	return gtkterm_configuration_set_status (self, rc, error);
}

/**
 * @brief Set the config option in the keyfile.
 *
 * All option which are given from the CLI are stored into the keyfile with [section]
 * Options are not saved to disk.
 *
 * @param name The configoption we want to set.
 *
 * @param value The value for this option.
 *
 * @param data The section we want to get the config from.
 *
 * @param error Error (not used).
 *
 * @return The inversed (0 -> 1, 1 -> 0) result of the operation. Because of the handling of
 * 	 		the return value from GOptionEntry. GOptionEntry contuinues if callback returns 1.
 *
 */
GtkTermConfigurationState on_set_config_options(const char *name, const char *value, gpointer data, GError **error) {

	int item_counter = 0;
	char *section = GTKTERM_APP(data)->section;
	GtkTermConfigurationState rc = GTKTERM_CONFIGURATION_SUCCESS;
	GtkTermConfiguration *self = GTKTERM_APP(data)->config;
	GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private(self);

	/** First check and load the keyfile */
	if ((rc =check_keyfile(self, section)) != GTKTERM_CONFIGURATION_SUCCESS)
		return rc;

	/**
	 * Check if we use the long or short option.
	 * For the long option, the option start at position 3 (--option). So add 2.
	 * For the short option the option start at positon 2 (-o) so add 1.
	 */
	name += strlen(name) > 2 ? 2 : 1;

	/** Search index for the option we want to set */
	while (item_counter < CONF_ITEM_LAST) {
		if (!g_strcmp0(name, GtkTermConfigurationItems[item_counter]) ||
			!g_strcmp0(name, GtkTermCLIShortOption[item_counter]))
			break;

		item_counter++;
	}

	switch (item_counter) {
		case CONF_ITEM_TERM_ECHO:
		case CONF_ITEM_SERIAL_DISABLE_PORT_LOCK:
		case CONF_ITEM_SERIAL_PARITY:
		case CONF_ITEM_SERIAL_FLOW_CONTROL:
			g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[item_counter], value);
			break;

		case CONF_ITEM_SERIAL_BAUDRATE:
			g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[item_counter], atol(value));
			break;

		case CONF_ITEM_SERIAL_BITS:
		case CONF_ITEM_SERIAL_STOPBITS:
		case CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX:
		case CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX:
		case CONF_ITEM_TERM_WAIT_CHAR:
		case CONF_ITEM_TERM_WAIT_DELAY:
		case CONF_ITEM_TERM_COLS:
		case CONF_ITEM_TERM_ROWS:				
			g_key_file_set_integer(priv->key_file, section, GtkTermConfigurationItems[item_counter], atoi(value));
			break;

		case CONF_ITEM_TERM_RAW_FILENAME:
		case CONF_ITEM_SERIAL_PORT:
			/** Check for max path length. Exit if it is to long.
			 * Note: Serial port is also a path to a device.
			 */
			if ((int)strlen(value) < PATH_MAX) {
				g_key_file_set_string(priv->key_file, section, GtkTermConfigurationItems[item_counter], value);			
			} else {
				g_printf(_("Filename to long\n\n"));
				*error = g_error_new (g_quark_from_static_string ("GTKTERM_CONFIGURATION"),
                             GTKTERM_CONFIGURATION_FILNAME_TO_LONG,
                             _ ("Filename to long (%d)"),
							 (int)strlen(value)
                             );				

				rc = GTKTERM_CONFIGURATION_FILNAME_TO_LONG;
			}

			break;

		default:
			/** We should not get here. */
			*error = g_error_new (g_quark_from_static_string ("GTKTERM_CONFIGURATION"),
                            	GTKTERM_CONFIGURATION_UNKNOWN_OPTION,
                             	_("Unknown option (%s)"),
							 	name
                             	);		
			rc = GTKTERM_CONFIGURATION_UNKNOWN_OPTION;
			break;
	}

	if (rc == GTKTERM_CONFIGURATION_SUCCESS)
		gtkterm_configuration_validate(GTKTERM_APP(data)->config, section);

	/** Set the dirty flag */
	priv->config_is_dirty = true;

	return (gtkterm_configuration_set_status(self, rc, *error) == GTKTERM_CONFIGURATION_SUCCESS);
}

/**
 * @brief Convert the colors RGB to internal color scheme
 *
 * @param color The composed color
 *
 * @param R The red component
 *
 * @param G The green component
 *
 * @param B The blue component
 *
 * @param A Alpha
 *
 */
//! Convert the colors RGB to internal color scheme
static void set_color(GdkRGBA *color, float R, float G, float B, float A)
{
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

/**
 * @brief  Sets the status and error of the last operation.
 *
 * @param self The configuration for which the get the status for.
 *
 * @param status The status to be set.
 * 
 * @param error The error message (can be NULL)
 *
 * @return  The latest status.
 *
 */
GtkTermConfigurationState gtkterm_configuration_set_status (GtkTermConfiguration *self, GtkTermConfigurationState status, GError *error) {
    GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private (self);

	priv->config_status = status;

	/** If there is a previous error, clear it */
	if (priv->config_error != NULL)
		g_error_free (priv->config_error);
	
	priv->config_error = error;	

	return status;
}

/**
 * @brief  Return the latest status condiation for the file operation.
 *
 * @param self The configuration for which the get the status for.
 *
 * @return  The latest status.
 *
 */
GtkTermConfigurationState gtkterm_configuration_get_status (GtkTermConfiguration *self) {
    GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private (self);

	return (priv->config_status);
}

/**
 * @brief  Return the latest error for the file operation.
 *
 * @param self The configuration for which the get the status for.
 *
 * @return  The latest error.
 *
 */
GError *gtkterm_configuration_get_error (GtkTermConfiguration *self) {
    GtkTermConfigurationPrivate *priv = gtkterm_configuration_get_instance_private (self);

	return (priv->config_error);
}