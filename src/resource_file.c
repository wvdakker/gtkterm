/***********************************************************************/
/* resource_config.c                                                   */
/* -----------------                                                   */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Save and load configuration from resource file                 */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 2.0 : Remove parsecfg. Switch to GKeyFile                    */
/*              Migration done by Jens Georg (phako)                   */
/*                                                                     */
/***********************************************************************/

#include <stdio.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <pango/pango-font.h>

#include <config.h>

//#include "i18n.h"
#include "serial.h"
#include "term_config.h"
#include "resource_file.h"
#include "i18n.h"
#include "interface.h"

#define CONFIGURATION_FILENAME ".gtktermrc"
GFile *config_file;

void config_file_init(void)
{
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
	config_file = g_file_new_build_filename(g_get_user_config_dir(), CONFIGURATION_FILENAME, NULL);

	if (!g_file_query_exists(config_file, NULL) && g_file_query_exists(config_file_old, NULL))
		g_file_move(config_file_old, config_file, G_FILE_COPY_NONE, NULL, NULL, NULL, NULL);
}

/* Dumps the section to the command line */
/* We will use this with auto package testing within Debian */
void dump_configuration_to_cli (char *section) {
	char str_buffer[32];

	i18n_printf (_("Configuration loaded from file: [%s]\n"), section);
	i18n_printf (_("\nSerial port\n"));
	i18n_printf (_("Port                     : %s\n"), port_conf.port);
	i18n_printf (_("Speed                    : %ld\n"), port_conf.speed);
	i18n_printf (_("Bits                     : %d\n"), port_conf.bits);
	i18n_printf (_("Stopbits                 : %d\n"), port_conf.stops);

	switch (port_conf.parity) {
		case 0:
			strcpy (str_buffer, _("none"));
			break;
		case 1:
			strcpy (str_buffer, _("odd"));
			break;
		case 2:
			strcpy (str_buffer, _("even"));
			break;
		default: // May never get here
			strcpy (str_buffer, _("unknown"));
	}
	i18n_printf (_("Parity                   : %s\n"), str_buffer);

	switch (port_conf.flow_control) {
		case 0:
			strcpy (str_buffer, _("none"));
			break;
		case 1:
			strcpy (str_buffer, _("xon"));
			break;
		case 2:
			strcpy (str_buffer, _("xoff"));
			break;
		case 3:
			strcpy (str_buffer, _("rs485"));
			break;
		default: // May never get here
			strcpy (str_buffer, _("unknown"));
	}
	i18n_printf (_("Flow control             : %s\n"), str_buffer);
	i18n_printf (_("RS485 RTS time before TX : %d\n"), port_conf.rs485_rts_time_before_transmit);
	i18n_printf (_("RS485 RTS time after TX  : %d\n"), port_conf.rs485_rts_time_after_transmit);
	i18n_printf (_("Disable port lock        : %s\n"), port_conf.disable_port_lock ? "True" : "False");

	i18n_printf (_("\nTerminal\n"));
	i18n_printf (_("Font                     : %s\n"), pango_font_description_to_string (term_conf.font));
	i18n_printf (_("Echo                     : %s\n"), term_conf.echo ? _("True") : _("False"));
	i18n_printf (_("CRLF                     : %s\n"), term_conf.crlfauto ? _("True") : _("False"));
	i18n_printf (_("Wait delay               : %d\n"), term_conf.delay);
	i18n_printf (_("Wait char                : %d\n"), term_conf.char_queue);
	i18n_printf (_("Timestamp                : %s\n"), term_conf.timestamp ? _("True") : _("False"));
	i18n_printf (_("Block cursor             : %s\n"), term_conf.block_cursor ? _("True") : _("False"));
	i18n_printf (_("Show cursor              : %s\n"), term_conf.show_cursor ? _("True") : _("False"));
	i18n_printf (_("Rows                     : %d\n"), term_conf.rows);
	i18n_printf (_("Cols                     : %d\n"), term_conf.columns);
	i18n_printf (_("Scrollback               : %d\n"), term_conf.scrollback);
	i18n_printf (_("Visual bell              : %s\n"), term_conf.visual_bell ? _("True") : _("False"));
	i18n_printf (_("Background color red     : %f\n"), term_conf.background_color.red);
	i18n_printf (_("Background color blue    : %f\n"), term_conf.background_color.blue);
	i18n_printf (_("Background color green   : %f\n"), term_conf.background_color.green);
	i18n_printf (_("Background color alpha   : %f\n"), term_conf.background_color.alpha);
	i18n_printf (_("Foreground color red     : %f\n"), term_conf.foreground_color.red);
	i18n_printf (_("Foreground color blue    : %f\n"), term_conf.foreground_color.blue);
	i18n_printf (_("Foreground color green   : %f\n"), term_conf.foreground_color.green);
	i18n_printf (_("Foreground color alpha   : %f\n"), term_conf.foreground_color.alpha);

	i18n_printf (_("\nMacro's\n"));
}

/* Save section configuration to file */
void save_configuration_to_file(GKeyFile *config, const char *section)
{
	char *string = NULL;
	GError *error = NULL;

	copy_configuration(config, section);
	g_key_file_save_to_file (config, g_file_get_path(config_file), &error);

	string = g_strdup_printf(_("Configuration [%s] saved\n"), section);
	show_message(string, MSG_WRN);
	g_free(string);
}

/* Load the configuration from <section> into the port and term config */
/* If it does not exists it creates one from the defaults              */
int load_configuration_from_file(const char *section)
{
	GKeyFile *config_object = NULL;
	GError *error = NULL;
	char *str = NULL;
	int value = 0;

	char *string = NULL;

	config_object = g_key_file_new ();
	if (!g_key_file_load_from_file (config_object, g_file_get_path (config_file), G_KEY_FILE_NONE, &error)) {
		g_debug ("Failed to load configuration file: %s", error->message);
		g_error_free (error);
		g_key_file_unref (config_object);

		return -1;
	}

	if (!g_key_file_has_group (config_object, section)) {
		string = g_strdup_printf(_("No section \"%s\" in configuration file\n"), section);
		show_message(string, MSG_ERR);
		g_free(string);
		g_key_file_unref (config_object);
		return -1;
	}

	hard_default_configuration();
	str = g_key_file_get_string (config_object, section, "port", NULL);
	if (str != NULL) {
		g_strlcpy (port_conf.port, str, sizeof (port_conf.port) - 1);
		g_free (str);
	}

	value = g_key_file_get_integer (config_object, section, "speed", NULL);
	if (value != 0) {
		port_conf.speed = value;
	}

	value = g_key_file_get_integer (config_object, section, "bits", NULL);
	if (value != 0) {
		port_conf.bits = value;
	}

	value = g_key_file_get_integer (config_object, section, "stopbits", NULL);
	if (value != 0) {
		port_conf.stops = value;
	}

	str = g_key_file_get_string (config_object, section, "parity", NULL);
	if (str != NULL) {
		if(!g_ascii_strcasecmp(str, "none"))
			port_conf.parity = 0;
		else if(!g_ascii_strcasecmp(str, "odd"))
			port_conf.parity = 1;
		else if(!g_ascii_strcasecmp(str, "even"))
			port_conf.parity = 2;
		g_free (str);
	}

	str = g_key_file_get_string (config_object, section, "flow_control", NULL);
	if (str != NULL) {
		if(!g_ascii_strcasecmp(str, "none"))
			port_conf.flow_control = 0;
		else if(!g_ascii_strcasecmp(str, "xon"))
			port_conf.flow_control = 1;
		else if(!g_ascii_strcasecmp(str, "rts"))
			port_conf.flow_control = 2;
		else if(!g_ascii_strcasecmp(str, "rs485"))
			port_conf.flow_control = 3;
    		
		g_free (str);
	}

	term_conf.delay = g_key_file_get_integer (config_object, section, "wait_delay", NULL);

	value = g_key_file_get_integer (config_object, section, "wait_char", NULL);
	if (value != 0) {
		term_conf.char_queue = (signed char) value;
	} else {
		term_conf.char_queue = -1;
	}
    
	port_conf.rs485_rts_time_before_transmit = g_key_file_get_integer (config_object, section, "rs485_rts_time_before_tx", NULL);
	port_conf.rs485_rts_time_after_transmit = g_key_file_get_integer (config_object, section, "rs485_rts_time_after_tx", NULL);
	term_conf.echo = g_key_file_get_boolean (config_object, section, "echo", NULL);
	term_conf.crlfauto = g_key_file_get_boolean (config_object, section, "crlfauto", NULL);
	port_conf.disable_port_lock = g_key_file_get_boolean (config_object, section, "disable_port_lock", NULL);

	g_clear_pointer (&term_conf.font, pango_font_description_free);
	str = g_key_file_get_string (config_object, section, "font", NULL);
	term_conf.font = pango_font_description_from_string (str);
	g_free (str);

	/* FIXME: Fix macros */
//	remove_shortcuts ();

	term_conf.show_cursor = g_key_file_get_boolean (config_object, section, "term_show_cursor", NULL);
	term_conf.rows = g_key_file_get_integer (config_object, section, "term_rows", NULL);
	term_conf.columns = g_key_file_get_integer (config_object, section, "term_columns", NULL);

	value = g_key_file_get_integer (config_object, section, "term_scrollback", NULL);
	if (value != 0) {
		term_conf.scrollback = value;
	}

	term_conf.visual_bell = g_key_file_get_boolean (config_object, section, "term_visual_bell", NULL);
	term_conf.foreground_color.red = g_key_file_get_double (config_object, section, "term_foreground_red", NULL);
	term_conf.foreground_color.green = g_key_file_get_double (config_object, section, "term_foreground_green", NULL);
	term_conf.foreground_color.blue = g_key_file_get_double (config_object, section, "term_foreground_blue", NULL);
	term_conf.foreground_color.alpha = g_key_file_get_double (config_object, section, "term_foreground_alpha", NULL);
	term_conf.background_color.red = g_key_file_get_double (config_object, section, "term_background_red", NULL);
	term_conf.background_color.green = g_key_file_get_double (config_object, section, "term_background_green", NULL);
	term_conf.background_color.blue = g_key_file_get_double (config_object, section, "term_background_blue", NULL);
	term_conf.background_color.alpha = g_key_file_get_double (config_object, section, "term_background_alpha", NULL);

	// @@TODO put in term_conf.c after loading the file
//	vte_terminal_set_font (VTE_TERMINAL(display), term_conf.font);

//	vte_terminal_set_size (VTE_TERMINAL(display), term_conf.rows, term_conf.columns);
//	vte_terminal_set_scrollback_lines (VTE_TERMINAL(display), term_conf.scrollback);
//	vte_terminal_set_color_foreground (VTE_TERMINAL(display), (const GdkRGBA *)&term_conf.foreground_color);
//	vte_terminal_set_color_background (VTE_TERMINAL(display), (const GdkRGBA *)&term_conf.background_color);
//	gtk_widget_queue_draw(display);

	return 0;
}

int check_configuration_file(void)
{
	struct stat my_stat;
	char *string = NULL;

	/* is configuration file present ? */
	if(stat(g_file_get_path(config_file), &my_stat) == 0)
	{
		/* If bad configuration file, fallback to _hardcoded_ defaults! */
		if(load_configuration_from_file("default") == -1)
		{
			hard_default_configuration();
			return -1;
		}
	} /* if not, create it, with the [default] section */
	else
	{
		GKeyFile *config;
		GError *error = NULL;

		string = g_strdup_printf(_("Configuration file (%s) with [default] configuration has been created.\n"), g_file_get_path(config_file));
		show_message(string, MSG_WRN);
		g_free(string);

		config = g_key_file_new ();
		hard_default_configuration();
		copy_configuration(config, "default");

		if (!g_key_file_save_to_file (config, g_file_get_path(config_file), &error)) {
			g_debug ("Error saving config file: %s", error->message);
			g_error_free (error);
		}

		g_key_file_unref (config);
	}

	return 0;
}

void copy_configuration(GKeyFile *configrc, const char *section)
{
	char *string = NULL;

	g_key_file_set_string (configrc, section, "port", port_conf.port);
	g_key_file_set_integer (configrc, section, "speed", port_conf.speed);
	g_key_file_set_integer (configrc, section,"bits", port_conf.bits);
	g_key_file_set_integer (configrc, section, "stopbits", port_conf.stops);

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

	g_key_file_set_string (configrc, section, "parity", string);
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

	g_key_file_set_string (configrc, section, "flow_control", string);
	g_free(string);

	g_key_file_set_integer (configrc, section, "wait_delay", term_conf.delay);
	g_key_file_set_integer (configrc, section, "wait_char", term_conf.char_queue);
	g_key_file_set_integer (configrc, section, "rs485_rts_time_before_tx",
    	                        port_conf.rs485_rts_time_before_transmit);
	g_key_file_set_integer (configrc, section, "rs485_rts_time_after_tx",
    	                        port_conf.rs485_rts_time_after_transmit);

	g_key_file_set_boolean (configrc, section, "echo", term_conf.echo);
	g_key_file_set_boolean (configrc, section, "crlfauto", term_conf.crlfauto);
	g_key_file_set_boolean (configrc, section, "disable_port_lock", port_conf.disable_port_lock);

	string = pango_font_description_to_string (term_conf.font);
	g_key_file_set_string (configrc, section, "font", string);
	g_free(string);

    /* FIXME: Fix macros! */
#if 0
	macros = get_shortcuts(&size);
	for(i = 0; i < size; i++)
	{
		string = g_strdup_printf("%s::%s", macros[i].shortcut, macros[i].action);
		cfgStoreValue(cfg, "macros", string, CFG_INI, pos);
		g_free(string);
	}
#endif

	g_key_file_set_boolean (configrc, section, "term_show_cursor", term_conf.show_cursor);
	g_key_file_set_integer (configrc, section, "term_rows", term_conf.rows);
	g_key_file_set_integer (configrc, section, "term_columns", term_conf.columns);
	g_key_file_set_integer (configrc, section, "term_scrollback", term_conf.scrollback);
	g_key_file_set_boolean (configrc, section, "term_visual_bell", term_conf.visual_bell);

	g_key_file_set_double (configrc, section, "term_foreground_red", term_conf.foreground_color.red);
	g_key_file_set_double (configrc, section, "term_foreground_green", term_conf.foreground_color.green);
	g_key_file_set_double (configrc, section, "term_foreground_blue", term_conf.foreground_color.blue);
	g_key_file_set_double (configrc, section, "term_foreground_alpha", term_conf.foreground_color.alpha);

	g_key_file_set_double (configrc, section, "term_background_red", term_conf.background_color.red);
	g_key_file_set_double (configrc, section, "term_background_green", term_conf.background_color.green);
	g_key_file_set_double (configrc, section, "term_background_blue", term_conf.background_color.blue);
	g_key_file_set_double (configrc, section, "term_background_alpha", term_conf.background_color.alpha);
}

int remove_section(char *cfg_file, char *section)
{
	FILE *f = NULL;
	char *buffer = NULL;
	char *buf;
	size_t size;
	char *to_search;
	size_t i, j, length, sect;

	f = fopen(cfg_file, "r");
	if(f == NULL)
	{
		perror(cfg_file);
		return -1;
	}

	fseek(f, 0L, SEEK_END);
	size = ftell(f);
	rewind(f);

	buffer = g_malloc(size);
	if(buffer == NULL)
	{
		perror("malloc");
		return -1;
	}

	if(fread(buffer, 1, size, f) != size)
	{
		perror(cfg_file);
		fclose(f);
		return -1;
	}

	to_search = g_strdup_printf("[%s]", section);
	length = strlen(to_search);

	/* Search section */
	for(i = 0; i < size - length; i++)
	{
		for(j = 0; j < length; j++)
		{
			if(to_search[j] != buffer[i + j])
				break;
		}

		if(j == length)
			break;
	}

	if(i == size - length)
	{
		i18n_printf(_("Cannot find section %s\n"), to_search);
		return -1;
	}

	sect = i;

	/* Search for next section */
	for(i = sect + length; i < size; i++)
	{
		if(buffer[i] == '[')
			break;
	}

	f = fopen(cfg_file, "w");
	if(f == NULL)
	{
		perror(cfg_file);
		return -1;
	}

	fwrite(buffer, 1, sect, f);
	buf = buffer + i;
	fwrite(buf, 1, size - i, f);
	fclose(f);

	g_free(to_search);
	g_free(buffer);

	return 0;
}

void hard_default_configuration(void)
{
	g_strlcpy(port_conf.port, DEFAULT_PORT, sizeof (port_conf.port));
	port_conf.speed = DEFAULT_SPEED;
	port_conf.parity = DEFAULT_PARITY;
	port_conf.bits = DEFAULT_BITS;
	port_conf.stops = DEFAULT_STOP;
	port_conf.flow_control = DEFAULT_FLOW;
	port_conf.rs485_rts_time_before_transmit = DEFAULT_DELAY_RS485;
	port_conf.rs485_rts_time_after_transmit = DEFAULT_DELAY_RS485;
	port_conf.disable_port_lock = FALSE;

	term_conf.char_queue = DEFAULT_CHAR;
	term_conf.delay = DEFAULT_DELAY;
	term_conf.echo = DEFAULT_ECHO;
	term_conf.crlfauto = FALSE;
	term_conf.timestamp = FALSE;
	term_conf.font = pango_font_description_from_string (DEFAULT_FONT);
	term_conf.block_cursor = TRUE;
	term_conf.show_cursor = TRUE;
	term_conf.rows = 80;
	term_conf.columns = 25;
	term_conf.scrollback = DEFAULT_SCROLLBACK;
	term_conf.visual_bell = TRUE;

	set_color (&term_conf.foreground_color, 0.66, 0.66, 0.66, 1.0);
	set_color (&term_conf.background_color, 0, 0, 0, 1.0);
}

void validate_configuration(void)
{
	char *string = NULL;

	switch(port_conf.speed)
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
			string = g_strdup_printf(_("Baudrate %ld may not be supported by all hardware"), port_conf.speed);
			show_message(string, MSG_ERR);
	    
			g_free(string);
	}

	if(port_conf.stops != 1 && port_conf.stops != 2)
	{
		string = g_strdup_printf(_("Invalid number of stop-bits: %d\nFalling back to default number of stop-bits number: %d\n"), port_conf.stops, DEFAULT_STOP);
		show_message(string, MSG_ERR);
		port_conf.stops = DEFAULT_STOP;

		g_free(string);
	}

	if(port_conf.bits < 5 || port_conf.bits > 8)
	{
		string = g_strdup_printf(_("Invalid number of bits: %d\nFalling back to default number of bits: %d\n"), port_conf.bits, DEFAULT_BITS);
		show_message(string, MSG_ERR);
		port_conf.bits = DEFAULT_BITS;

		g_free(string);
	}

	if(term_conf.delay < 0 || term_conf.delay > 500)
	{
		string = g_strdup_printf(_("Invalid delay: %d ms\nFalling back to default delay: %d ms\n"), term_conf.delay, DEFAULT_DELAY);
		show_message(string, MSG_ERR);
		term_conf.delay = DEFAULT_DELAY;

		g_free(string);
	}

	if(term_conf.font == NULL)
		term_conf.font = pango_font_description_from_string (DEFAULT_FONT);
}

void set_color(GdkRGBA *color, float R, float G, float B, float A)
{
	color->red = R;
	color->green = G;
	color->blue = B;
	color->alpha = A;
}
