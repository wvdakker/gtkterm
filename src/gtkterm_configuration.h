/***********************************************************************
 * gtkterm_configuration.h
 *
 * GTKTerm Software (c) Julien Schmitt
 *
 * 
 * @brief Purpose 
 *      Load and save configuration file
 *      - Header file
 *
 ***********************************************************************/

#ifndef GTKTERM_CONFIGURATION_H
#define GTKTERM_CONFIGURATION_H

/**
 * @brief  Enum items for configuration
 * 
 * Define all configuration items which are used
 * in the resource file. it is an index to ConfigurationItem.
 * Configuration item names. 
 */
enum {
		CONF_ITEM_SERIAL_PORT,
		CONF_ITEM_SERIAL_BAUDRATE,
		CONF_ITEM_SERIAL_BITS,
		CONF_ITEM_SERIAL_STOPBITS,
		CONF_ITEM_SERIAL_PARITY,
		CONF_ITEM_SERIAL_FLOW_CONTROL,
		CONF_ITEM_TERM_WAIT_DELAY,
		CONF_ITEM_TERM_WAIT_CHAR,
		CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX,
		CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX,
		CONF_ITEM_TERM_MACROS,
		CONF_ITEM_TERM_RAW_FILENAME,
		CONF_ITEM_TERM_ECHO,
		CONF_ITEM_TERM_CRLF_AUTO,
		CONF_ITEM_SERIAL_DISABLE_PORT_LOCK,
		CONF_ITEM_TERM_FONT,
		CONF_ITEM_TERM_TIMESTAMP,		
		CONF_ITEM_TERM_BLOCK_CURSOR,		
		CONF_ITEM_TERM_SHOW_CURSOR,
		CONF_ITEM_TERM_ROWS,
		CONF_ITEM_TERM_COLS,
		CONF_ITEM_TERM_SCROLLBACK,
		CONF_ITEM_TERM_VISUAL_BELL,
		CONF_ITEM_TERM_FOREGROUND_RED,
		CONF_ITEM_TERM_FOREGROUND_GREEN,
		CONF_ITEM_TERM_FOREGROUND_BLUE,
		CONF_ITEM_TERM_FOREGROUND_ALPHA,
		CONF_ITEM_TERM_BACKGROUND_RED,
		CONF_ITEM_TERM_BACKGROUND_GREEN,
		CONF_ITEM_TERM_BACKGROUND_BLUE,
		CONF_ITEM_TERM_BACKGROUND_ALPHA,	
		CONF_ITEM_LAST						/**< Checking as last item in the list.		*/
};

/**
 * @brief  Enum config_error id.
 * 
 * Many of the gtk_configuration functions return
 * an error id.
 */
typedef enum {
	GTKTERM_CONFIGURATION_SUCCESS,
	GTKTERM_CONFIGURATION_FILE_CONFIG_LOAD,
	GTKTERM_CONFIGURATION_FILE_NOT_FOUND,
	GTKTERM_CONFIGURATION_FILE_CREATED,
	GTKTERM_CONFIGURATION_FILE_SAVED,
	GTKTERM_CONFIGURATION_FILE_NOT_SAVED,
	GTKTERM_CONFIGURATION_NO_KEYFILE_LOADED,
	GTKTERM_CONFIGURATION_SECTION_REMOVED,
	GTKTERM_CONFIGURATION_SECTION_NOT_REMOVED,
	GTKTERM_CONFIGURATION_SECTION_UNKNOWN,
	GTKTERM_CONFIGURATION_INVALID_BAUDRATE,
	GTKTERM_CONFIGURATION_INVALID_BITS,
	GTKTERM_CONFIGURATION_INVALID_STOPBITS,
	GTKTERM_CONFIGURATION_INVALID_DELAY,
	GTKTERM_CONFIGURATION_FILNAME_TO_LONG,
	GTKTERM_CONFIGURATION_UNKNOWN_OPTION,
	GTKTERM_CONFIGURATION_LAST

} GtkTermConfigurationState;

extern const char GtkTermConfigurationItems [][CONF_ITEM_LENGTH];

G_BEGIN_DECLS

#define GTKTERM_TYPE_CONFIGURATION gtkterm_configuration_get_type ()
G_DECLARE_FINAL_TYPE (GtkTermConfiguration, gtkterm_configuration, GTKTERM, CONFIGURATION, GObject)
typedef struct _GtkTermConfiguration GtkTermConfiguration;

GtkTermConfiguration *gtkterm_configuration_new (void);

GtkTermConfigurationState on_set_config_options (const char *, const char *, gpointer,  GError **);
GtkTermConfigurationState gtkterm_configuration_get_status (GtkTermConfiguration *);
GError *gtkterm_configuration_get_error (GtkTermConfiguration *);

G_END_DECLS

#endif //GTKTERM_CONFIGURATION