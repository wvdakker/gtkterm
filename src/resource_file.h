/***********************************************************************
 * resource_file.h                                              
 * ---------------                          
 *           GTKTerm Software                                   
 *                      (c) Julien Schmitt               
 *                                                               
 * ------------------------------------------------------------------- 
 *                                                            
 *   \brief Purpose                                        
 *      Load and save configuration file                              
 *      - Header file -                                       
 *                                                           
 ***********************************************************************/

#ifndef RESOURCE_FILE_H_
#define RESOURCE_FILE_H_

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
	CONF_ERROR_SUCCESS,
	CONF_ERROR_FILE_CONFIG_LOAD,
	CONF_ERROR_FILE_NOT_FOUND,
	CONF_ERROR_FILE_CREATED,
	CONF_ERROR_FILE_SAVED,
	CONF_ERROR_FILE_NOT_SAVED,
	CONF_ERROR_NO_KEYFILE_LOADED,
	CONF_ERROR_SECTION_REMOVED,
	CONF_ERROR_SECTION_NOT_REMOVED,
	CONF_ERROR_SECTION_UNKNOWN,
	CONF_ERROR_INVALID_BAUDRATE,
	CONF_ERROR_INVALID_BITS,		
	CONF_ERROR_INVALID_STOPBITS,
	CONF_ERROR_INVALID_DELAY,
	CONF_ERROR_LAST

} GtkTermConfigStatus;

extern const char GtkTermConfigurationItems [][CONF_ITEM_LENGTH];

G_BEGIN_DECLS

#define GTKTERM_TYPE_CONFIGURATION gtkterm_configuration_get_type ()
G_DECLARE_FINAL_TYPE (GtkTermConfiguration, gtkterm_configuration, GTKTERM, CONFIGURATION, GObject)
typedef struct _GtkTermConfiguration GtkTermConfiguration;

GtkTermConfiguration *gtkterm_configuration_new (void);

GtkTermConfigStatus on_set_config_options (const char *, const char *, gpointer,  GError **);
GtkTermConfigStatus gtkterm_configuration_status (GtkTermConfiguration *); /**< \todo: Add GError output somewhere... */

G_END_DECLS

#endif