/************************************************************************/
/* resource_file_conv.c                                                 */
/* --------------------                                                 */
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
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <pango/pango-font.h>

#include <config.h>

#include "gtkterm_struct.h"
#include "resource_file_conv.h"
#include "macros.h"

//! Default configuration filename
#define CONFIGURATION_FILENAME ".gtktermrc"
#define CONFIGURATION_FILENAME_V1 ".gtktermrc.v1"
//! The key file
GFile *config_file;

//! Define all configuration items which are used
//! in the resource file. it is an index to ConfigurationItem.
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
		CONF_ITEM_TERM_AUTO_LF,
		CONF_ITEM_TERM_AUTO_CR,
		CONF_ITEM_SERIAL_DISABLE_PORT_LOCK,
		CONF_ITEM_TERM_FONT,
		CONF_ITEM_TERM_TIMESTAMP,		
		CONF_ITEM_TERM_BLOCK_CURSOR,		
		CONF_ITEM_TERM_SHOW_CURSOR,
		CONF_ITEM_TERM_ROWS,
		CONF_ITEM_TERM_COLS,
		CONF_ITEM_TERM_SCROLLBACK,
		CONF_ITEM_TERM_INDEX,
		CONF_ITEM_TERM_VIEW_MODE,
		CONF_ITEM_TERM_HEX_CHARS,
		CONF_ITEM_TERM_VISUAL_BELL,		
		CONF_ITEM_TERM_FOREGROUND_RED,
		CONF_ITEM_TERM_FOREGROUND_GREEN,
		CONF_ITEM_TERM_FOREGROUND_BLUE,
		CONF_ITEM_TERM_FOREGROUND_ALPHA,
		CONF_ITEM_TERM_BACKGROUND_RED,
		CONF_ITEM_TERM_BACKGROUND_GREEN,
		CONF_ITEM_TERM_BACKGROUND_BLUE,
		CONF_ITEM_TERM_BACKGROUND_ALPHA,	
		CONF_ITEM_LAST						//! Checking as last item in the list.
};

// Used configuration options to hold consistency between load/save functions
const char ConfigurationItem [][32] = {
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
		"term_auto_lf",
		"term_auto_cr",
		"disable_port_lock",
		"term_font",
		"term_show_timestamp",
		"term_block_cursor",				
		"term_show_cursor",
		"term_rows",
		"term_columns",
		"term_scrollback",
		"term_index",		
		"term_view_mode",
		"term_hex_chars",				
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

void config_file_init(void) {
	/*
	 * Old location of configuration file was $HOME/.gtktermrc
	 * New location is $XDG_CONFIG_HOME/.gtktermrc
	 *
	 * If configuration file exists at new location, use that one.
	 * Otherwise, if file exists at old location, move file to new location.
	 *
         * Version 2.0: Because we have to use gtkterm_conv, the file is always at
	 * the user directory. So we can skip eventually moving the file.
	 */
	GFile *config_file_old = g_file_new_build_filename(getenv("HOME"), CONFIGURATION_FILENAME, NULL);
	GFile *config_file_v1 = g_file_new_build_filename(g_get_user_config_dir(), CONFIGURATION_FILENAME_V1, NULL);
	config_file = g_file_new_build_filename(g_get_user_config_dir(), CONFIGURATION_FILENAME, NULL);

	if (!g_file_query_exists(config_file, NULL) && g_file_query_exists(config_file_old, NULL))
		g_file_move(config_file_old, config_file, G_FILE_COPY_NONE, NULL, NULL, NULL, NULL);

	// Save the original file
	g_printf(_("Your original file will be backuped to %s\n"), g_file_get_path (config_file_v1));
	g_file_copy(config_file, config_file_v1, G_FILE_COPY_BACKUP, NULL, NULL, NULL, NULL);
}

/* Save <section> configuration to file */
void save_configuration_to_file(GKeyFile *config) {
	GError *error = NULL;

	g_key_file_save_to_file (config, g_file_get_path(config_file), &error);

}

//! Copy the active configuration into <section> of the Key file
void copy_configuration(GKeyFile *configrc, const char *section)
{
	char *string = NULL;
	char **string_list = NULL;
	gsize nr_of_strings = 0;

	g_key_file_set_string (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_PORT], port_conf.port);
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_BAUDRATE], port_conf.baudrate);
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_BITS], port_conf.bits);
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_STOPBITS], port_conf.stopbits);

	switch(port_conf.parity)
	{
		case 0:
			string = g_strdup_printf("none");
			break;
		case 1:
			string = g_strdup_printf("odd");
			break;
		case 2:
			string = g_strdup_printf("even");
			break;
		default:
		    string = g_strdup_printf("none");
	}

	g_key_file_set_string (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_PARITY], string);
	g_free(string);

	switch(port_conf.flow_control)
	{
		case 0:
			string = g_strdup_printf("none");
			break;
		case 1:
			string = g_strdup_printf("xon");
			break;
		case 2:
			string = g_strdup_printf("rts");
			break;
		case 3:
			string = g_strdup_printf("rs485");
			break;
		default:
			string = g_strdup_printf("none");
	}

	g_key_file_set_string (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_FLOW_CONTROL], string);
	g_free(string);

	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_TERM_WAIT_DELAY], term_conf.delay);
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_TERM_WAIT_CHAR], term_conf.char_queue);
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_RS485_RTS_TIME_BEFORE_TX],
    	                        port_conf.rs485_rts_time_before_transmit);
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_RS485_RTS_TIME_AFTER_TX],
    	                        port_conf.rs485_rts_time_after_transmit);

	g_key_file_set_boolean (configrc, section, ConfigurationItem[CONF_ITEM_TERM_ECHO], term_conf.echo);
	g_key_file_set_boolean (configrc, section, ConfigurationItem[CONF_ITEM_TERM_AUTO_LF], term_conf.auto_lf);
	g_key_file_set_boolean (configrc, section, ConfigurationItem[CONF_ITEM_TERM_AUTO_CR], term_conf.auto_cr);
	g_key_file_set_boolean (configrc, section, ConfigurationItem[CONF_ITEM_SERIAL_DISABLE_PORT_LOCK], port_conf.disable_port_lock);

	string = pango_font_description_to_string (term_conf.font);
	g_key_file_set_string (configrc, section, ConfigurationItem[CONF_ITEM_TERM_FONT], string);
	g_free(string);

	//! Macros are an array of strings, so we have to convert it
	//! All macros ends up in the string_list
	string_list = g_malloc ( macro_count () * sizeof (char *) * 2 + 1);
	nr_of_strings = convert_macros_to_string (string_list);
	g_key_file_set_string_list (configrc, section, ConfigurationItem[CONF_ITEM_TERM_MACROS], (const char * const*) string_list, nr_of_strings);
	g_free(string_list);	

	g_key_file_set_boolean (configrc, section, ConfigurationItem[CONF_ITEM_TERM_SHOW_CURSOR], term_conf.show_cursor);
	g_key_file_set_boolean (configrc, section, ConfigurationItem[CONF_ITEM_TERM_BLOCK_CURSOR], term_conf.block_cursor);	
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_TERM_ROWS], term_conf.rows);
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_TERM_COLS], term_conf.columns);
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_TERM_SCROLLBACK], term_conf.scrollback);
	g_key_file_set_boolean (configrc, section, ConfigurationItem[CONF_ITEM_TERM_VISUAL_BELL], term_conf.visual_bell);
	g_key_file_set_boolean (configrc, section, ConfigurationItem[CONF_ITEM_TERM_TIMESTAMP], term_conf.timestamp);
	g_key_file_set_boolean (configrc, section, ConfigurationItem[CONF_ITEM_TERM_INDEX], term_conf.show_index);	

	g_key_file_set_string (configrc, section, ConfigurationItem[CONF_ITEM_TERM_VIEW_MODE], term_conf.view_mode == GTKTERM_TERMINAL_VIEW_ASCII ? "ASCII" : "HEX");
	g_key_file_set_integer (configrc, section, ConfigurationItem[CONF_ITEM_TERM_HEX_CHARS], term_conf.hex_chars);	

	g_key_file_set_double (configrc, section, ConfigurationItem[CONF_ITEM_TERM_FOREGROUND_RED], term_conf.foreground_color.red);
	g_key_file_set_double (configrc, section, ConfigurationItem[CONF_ITEM_TERM_FOREGROUND_GREEN], term_conf.foreground_color.green);
	g_key_file_set_double (configrc, section, ConfigurationItem[CONF_ITEM_TERM_FOREGROUND_BLUE], term_conf.foreground_color.blue);
	g_key_file_set_double (configrc, section, ConfigurationItem[CONF_ITEM_TERM_FOREGROUND_ALPHA], term_conf.foreground_color.alpha);

	g_key_file_set_double (configrc, section, ConfigurationItem[CONF_ITEM_TERM_BACKGROUND_RED], term_conf.background_color.red);
	g_key_file_set_double (configrc, section, ConfigurationItem[CONF_ITEM_TERM_BACKGROUND_GREEN], term_conf.background_color.green);
	g_key_file_set_double (configrc, section, ConfigurationItem[CONF_ITEM_TERM_BACKGROUND_BLUE], term_conf.background_color.blue);
	g_key_file_set_double (configrc, section, ConfigurationItem[CONF_ITEM_TERM_BACKGROUND_ALPHA], term_conf.background_color.alpha);
}

//! Create a new <default> configuration
void hard_default_configuration(void)
{
	g_strlcpy(port_conf.port, DEFAULT_PORT, sizeof (port_conf.port));
	port_conf.baudrate = DEFAULT_BAUDRATE;
	port_conf.parity = 0;
	port_conf.bits = DEFAULT_BITS;
	port_conf.stopbits = DEFAULT_STOPBITS;
	port_conf.flow_control = FALSE;
	port_conf.rs485_rts_time_before_transmit = DEFAULT_DELAY_RS485;
	port_conf.rs485_rts_time_after_transmit = DEFAULT_DELAY_RS485;
	port_conf.disable_port_lock = FALSE;

	term_conf.char_queue = DEFAULT_CHAR;
	term_conf.delay = DEFAULT_DELAY;
	term_conf.echo = DEFAULT_ECHO;
	term_conf.auto_cr = FALSE;
	term_conf.auto_lf = FALSE;
	term_conf.timestamp = FALSE;
	term_conf.font = pango_font_description_from_string (DEFAULT_FONT);
	term_conf.block_cursor = TRUE;
	term_conf.show_cursor = TRUE;
	term_conf.rows = 80;
	term_conf.columns = 25;
	term_conf.scrollback = DEFAULT_SCROLLBACK;
	term_conf.visual_bell = TRUE;
	term_conf.show_index = FALSE;	
	term_conf.view_mode = GTKTERM_TERMINAL_VIEW_ASCII;
	term_conf.hex_chars = 16;	

	set_color (&term_conf.foreground_color, 0.66, 0.66, 0.66, 1.0);
	set_color (&term_conf.background_color, 0, 0, 0, 1.0);
}

//! validate the active configuration
void validate_configuration(void)
{
	char *string = NULL;

	switch(port_conf.baudrate)
	{
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
			string = g_strdup_printf(_("Baudrate %ld may not be supported by all hardware"), port_conf.baudrate);
			show_message(string, GTK_MESSAGE_ERROR);
	    
			g_free(string);
	}

	if(port_conf.stopbits != 1 && port_conf.stopbits != 2)
	{
		string = g_strdup_printf(_("Invalid number of stop-bits: %d\nFalling back to default number of stop-bits number: %d\n"), port_conf.stopbits, DEFAULT_STOPBITS);
		show_message(string, GTK_MESSAGE_ERROR);
		port_conf.stopbits = DEFAULT_STOPBITS;

		g_free(string);
	}

	if(port_conf.bits < 5 || port_conf.bits > 8)
	{
		string = g_strdup_printf(_("Invalid number of bits: %d\nFalling back to default number of bits: %d\n"), port_conf.bits, DEFAULT_BITS);
		show_message(string, GTK_MESSAGE_ERROR);
		port_conf.bits = DEFAULT_BITS;

		g_free(string);
	}

	if(term_conf.delay < 0 || term_conf.delay > 500)
	{
		string = g_strdup_printf(_("Invalid delay: %d ms\nFalling back to default delay: %d ms\n"), term_conf.delay, DEFAULT_DELAY);
		show_message(string, GTK_MESSAGE_ERROR);
		term_conf.delay = DEFAULT_DELAY;

		g_free(string);
	}

	if(term_conf.font == NULL)
		term_conf.font = pango_font_description_from_string (DEFAULT_FONT);
}

//! Convert the colors RGB to internal color scheme
void set_color(GdkRGBA *color, float R, float G, float B, float A)
{
	color->red = R;
	color->green = G;
	color->blue = B;
	color->alpha = A;
}
