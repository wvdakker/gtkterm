#include <stdio.h>
#include <stdbool.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

 #include "gtkterm.h"
 #include "gtkterm_configuration.h"
 #include "gtkterm_messages.h"
    
 const char GtkTermErrorMessage [][GTKTERM_MESSAGE_LENGTH] = {
			N_("File operation successful"), 														/**< CONF_ERROR_SUCCESS			 	*/
			N_("Failed to load configuration file: %s"), 											/**< CONF_ERROR_FILE_CONFIG_LOAD 	*/
			N_("Configruation file not found"), 													/**< CONF_ERROR_FILE_NOT_FOUND   	*/
			N_("Configuration file with [default] configuration has been created and saved"), 		/**< CONF_ERROR_FILE_CREATED	 	*/
			N_("Configuration saved"), 																/**< CONF_ERROR_FILE_SAVED			*/
			N_("Failed to save configuration file: %s"), 											/**< CONF_ERROR_FILE_NOT_SAVED		*/
			N_("No keyfile loaded"), 																/**< CONF_ERROR_NO_KEYFILE_LOADED	*/
			N_("Section [%s] removed"), 															/**< CONF_ERROR_SECTION_REMOVED		*/
			N_("Failed to remove section: %s"), 													/**< CONF_ERROR_SECTION_NOT_REMOVED */
			N_("No section [%s] in configuration file"), 											/**< CONF_ERROR_SECTION_UNKNOWN		*/
			N_("Baudrate %ld may not be supported by all hardware"), 								/**< CONF_ERROR_INVALID_BAUDRATE	*/
			N_("Invalid number of bits: %d\nFalling back to default number of bits: %d"), 			/**< CONF_ERROR_INVALID_BITS		*/
			N_("Invalid number of stopbits: %d\nFalling back to default number of stopbits: %d"), 	/**< CONF_ERROR_INVALID_STOPBITS	*/
			N_("Invalid delay: %d ms\nFalling back to default delay: %d ms") 						/**< CONF_ERROR_INVALID_DELAY		*/
 };

/**
 * \todo fill in the %d and %s
 */
 const char *gtkterm_message (GtkTermConfigStatus error) {

    if (error >= CONF_ERROR_LAST) {
        return (_("Invalid message-id"));
    }

    return (GtkTermErrorMessage[error]);
 }
