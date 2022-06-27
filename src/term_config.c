/***********************************************************************/
/* term_config.c                                                       */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Configuration of the serial port                               */
/*                                                                     */
/*   ChangeLog                                                         */
/*		- 2.0	 : Refactor FR-> UK									   */
/*      - 0.99.7 : Refactor to use newer gtk widgets                   */
/*                 Add ability to use arbitrary baud                   */
/*                 Add rs458 capability - Marc Le Douarain             */
/*                 Remove auto cr/lf stuff - (use macros instead)      */
/*      - 0.99.5 : Make the combo list for the device editable         */
/*      - 0.99.3 : Configuration for VTE terminal                      */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.99.1 : fixed memory management bug                         */
/*                 test if there are devices found                     */
/*      - 0.99.0 : fixed enormous memory management bug ;-)            */
/*                 save / read macros                                  */
/*      - 0.98.5 : font saved in configuration                         */
/*                 bug fixed in memory management                      */
/*                 combos set to non editable                          */
/*      - 0.98.3 : configuration file                                  */
/*      - 0.98.2 : autodetect existing devices                         */
/*      - 0.98 : added devfs devices                                   */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <vte/vte.h>

#include "interface.h"
#include "term_config.h"
#include "parsecfg.h"
#include "macros.h"

#define CONFIGURATION_FILENAME ".gtktermrc"

GFile *config_file;
struct configuration_port config;
display_config_t term_conf;

/* Configuration file variables */
char **port;
int *speed;
int *bits;
int *stopbits;
char **parity;
char **flow;
int *wait_delay;
int *wait_char;
int *rts_time_before_tx;
int *rts_time_after_tx;
int *echo;
int *crlfauto;
int *timestamp;
cfgList **macro_list = NULL;
char **font;
int *block_cursor;
int *rows;
int *columns;
int *scrollback;
int *visual_bell;
float *foreground_red;
float *foreground_blue;
float *foreground_green;
float *foreground_alpha;
float *background_red;
float *background_blue;
float *background_green;
float *background_alpha;

cfgStruct cfg[] =
{
	{"port", CFG_STRING, &port},
	{"speed", CFG_INT, &speed},
	{"bits", CFG_INT, &bits},
	{"stopbits", CFG_INT, &stopbits},
	{"parity", CFG_STRING, &parity},
	{"flow", CFG_STRING, &flow},
	{"wait_delay", CFG_INT, &wait_delay},
	{"wait_char", CFG_INT, &wait_char},
	{"rs485_rts_time_before_tx", CFG_INT, &rts_time_before_tx},
	{"rs485_rts_time_after_tx", CFG_INT, &rts_time_after_tx},
	{"echo", CFG_BOOL, &echo},
	{"crlfauto", CFG_BOOL, &crlfauto},
	{"timestamp", CFG_BOOL, &timestamp},
	{"font", CFG_STRING, &font},
	{"macros", CFG_STRING_LIST, &macro_list},
	{"term_block_cursor", CFG_BOOL, &block_cursor},
	{"term_rows", CFG_INT, &rows},
	{"term_columns", CFG_INT, &columns},
	{"term_scrollback", CFG_INT, &scrollback},
	{"term_visual_bell", CFG_BOOL, &visual_bell},
	{"term_foreground_red", CFG_FLOAT, &foreground_red},
	{"term_foreground_blue", CFG_FLOAT, &foreground_blue},
	{"term_foreground_green", CFG_FLOAT, &foreground_green},
	{"term_foreground_alpha", CFG_FLOAT, &foreground_alpha},
	{"term_background_red", CFG_FLOAT, &background_red},
	{"term_background_blue", CFG_FLOAT, &background_blue},
	{"term_background_green", CFG_FLOAT, &background_green},
	{"term_background_alpha", CFG_FLOAT, &background_alpha},
	{NULL, CFG_END, NULL}
};

void set_color(GdkRGBA *, float, float, float, float);

void config_file_init(void)
{
	/*
	 * Old location of configuration file was $HOME/.gtktermrc
	 * New location is $XDG_CONFIG_HOME/.gtktermrc
	 *
	 * If configuration file exists at new location, use that one.
	 * Otherwise, if file exists at old location, move file to new location.
	 */
	GFile *config_file_old = g_file_new_build_filename(getenv("HOME"), CONFIGURATION_FILENAME, NULL);
	config_file = g_file_new_build_filename(g_get_user_config_dir(), CONFIGURATION_FILENAME, NULL);

	if (!g_file_query_exists(config_file, NULL) && g_file_query_exists(config_file_old, NULL))
		g_file_move(config_file_old, config_file, G_FILE_COPY_NONE, NULL, NULL, NULL, NULL);
}

int check_configuration_file(void)
{
	struct stat my_stat;
	char *string = NULL;

	/* is configuration file present ? */
	if (stat(g_file_get_path(config_file), &my_stat) == 0)
	{
		/* If bad configuration file, fallback to _hardcoded_ defaults! */
		if(load_configuration_from_file("default") == -1)
		{
			hard_default_configuration();
			return -1;
		}
	}
	else
	{ 	/* if not, create it, with the [default] section */
		string = g_strdup_printf(_("Configuration file (%s) with\n[default] configuration has been created.\n"), g_file_get_path(config_file));
		show_message(string, MSG_WRN);
		cfgAllocForNewSection(cfg, "default");
		hard_default_configuration();
		copy_configuration(0);
		cfgDump(g_file_get_path(config_file), cfg, CFG_INI, 1);

		g_free(string);
	}

	return 0;
}

void verify_configuration(void)
{
	char *string = NULL;

	switch(config.speed)
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
			string = g_strdup_printf(_("Baudrate %d may not be supported by all hardware"), config.speed);
			show_message(string, MSG_ERR);
			
			g_free(string);
	}

	if(config.stops != 1 && config.stops != 2)
	{
		string = g_strdup_printf(_("Invalid number of stop-bits: %d\nFalling back to default number of stop-bits number: %d\n"), config.stops, DEFAULT_STOP);
		show_message(string, MSG_ERR);
		config.stops = DEFAULT_STOP;
		
		g_free(string);
	}

	if(config.bits < 5 || config.bits > 8)
	{
		string = g_strdup_printf(_("Invalid number of bits: %d\nFalling back to default number of bits: %d\n"), config.bits, DEFAULT_BITS);
		show_message(string, MSG_ERR);
		config.bits = DEFAULT_BITS;
		
		g_free(string);
	}

	if(config.delay < 0 || config.delay > 500)
	{
		string = g_strdup_printf(_("Invalid delay: %d ms\nFalling back to default delay: %d ms\n"), config.delay, DEFAULT_DELAY);
		show_message(string, MSG_ERR);
		config.delay = DEFAULT_DELAY;

		g_free(string);
	}

	if(term_conf.font == NULL)
		term_conf.font = g_strdup_printf(DEFAULT_FONT);

}

int load_configuration_from_file(char *config_name)
{
	int max, i, j, k, size;
	gchar *string = NULL;
	gchar *str;
	macro_t *macros = NULL;
	cfgList *t;

	max = cfgParse(g_file_get_path(config_file), cfg, CFG_INI);

	if(max == -1)
	{
		show_message(_("Cannot read configuration file!\nConfig file may contain invalid parameter.\n"), MSG_ERR);
		return -1;
	}
	else
	{
		for(i = 0; i < max; i++)
		{
			if(!strcmp(config_name, cfgSectionNumberToName(i)))
			{
				hard_default_configuration();

				if(port[i] != NULL)
					strcpy(config.port, port[i]);
				if(speed[i] != 0)
					config.speed = speed[i];
				if(bits[i] != 0)
					config.bits = bits[i];
				if(stopbits[i] != 0)
					config.stops = stopbits[i];
				if(parity[i] != NULL)
				{
					if(!g_ascii_strcasecmp(parity[i], "none"))
						config.parity = 0;
					else if(!g_ascii_strcasecmp(parity[i], "odd"))
						config.parity = 1;
					else if(!g_ascii_strcasecmp(parity[i], "even"))
						config.parity = 2;
				}
				if(flow[i] != NULL)
				{
					if(!g_ascii_strcasecmp(flow[i], "none"))
						config.flux = 0;
					else if(!g_ascii_strcasecmp(flow[i], "xon"))
						config.flux = 1;
					else if(!g_ascii_strcasecmp(flow[i], "rts"))
						config.flux = 2;
					else if(!g_ascii_strcasecmp(flow[i], "rs485"))
						config.flux = 3;
				}

				config.delay = wait_delay[i];

				if(wait_char[i] != 0)
					config.char_queue = (signed char)wait_char[i];
				else
					config.char_queue = -1;

				config.rs485_rts_time_before_transmit = rts_time_before_tx[i];
				config.rs485_rts_time_after_transmit = rts_time_after_tx[i];

				if(echo[i] != -1)
					config.echo = (gboolean)echo[i];
				else
					config.echo = FALSE;

				if(crlfauto[i] != -1)
					config.crlfauto = (gboolean)crlfauto[i];
				else
					config.crlfauto = FALSE;

				if(timestamp[i] != -1)
					config.timestamp = (gboolean)timestamp[i];
				else
					config.timestamp = FALSE;

				g_free(term_conf.font);
				term_conf.font = g_strdup(font[i]);

				t = macro_list[i];
				size = 0;
				if(t != NULL)
				{
					size++;
					while(t->next != NULL)
					{
						t = t->next;
						size++;
					}
				}

				if(size != 0)
				{
					t = macro_list[i];
					macros = g_malloc(size * sizeof(macro_t));
					if(macros == NULL)
					{
						perror("malloc");
						return -1;
					}
					for(j = 0; j < size; j++)
					{
						for(k = 0; k < (strlen(t->str) - 1); k++)
						{
							if((t->str[k] == ':') && (t->str[k + 1] == ':'))
								break;
						}
						macros[j].shortcut = g_strndup(t->str, k);
						str = &(t->str[k + 2]);
						macros[j].action = g_strdup(str);

						t = t->next;
					}
				}

//				remove_shortcuts();
//				create_shortcuts(macros, size);

				g_free(macros);

				if(block_cursor[i] != -1)
					term_conf.block_cursor = (gboolean)block_cursor[i];
				else
					term_conf.block_cursor = TRUE;

				if(rows[i] != 0)
					term_conf.rows = rows[i];

				if(columns[i] != 0)
					term_conf.columns = columns[i];

				if(scrollback[i] != 0)
					term_conf.scrollback = scrollback[i];

				if(visual_bell[i] != -1)
					term_conf.visual_bell = (gboolean)visual_bell[i];
				else
					term_conf.visual_bell = FALSE;

				term_conf.foreground_color.red = foreground_red[i];
				term_conf.foreground_color.green = foreground_green[i];
				term_conf.foreground_color.blue = foreground_blue[i];
				term_conf.foreground_color.alpha = foreground_alpha[i];

				term_conf.background_color.red = background_red[i];
				term_conf.background_color.green = background_green[i];
				term_conf.background_color.blue = background_blue[i];
				term_conf.background_color.alpha = background_alpha[i];

				/* rows and columns are empty when the conf is autogenerate in the
				   first save; so set term to default */
				if(rows[i] == 0 || columns[i] == 0)
				{
					term_conf.block_cursor = TRUE;
					term_conf.rows = 80;
					term_conf.columns = 25;
					term_conf.scrollback = DEFAULT_SCROLLBACK;
					term_conf.visual_bell = FALSE;

					term_conf.foreground_color.red = 0.66;
					term_conf.foreground_color.green = 0.66;
					term_conf.foreground_color.blue = 0.66;
					term_conf.foreground_color.alpha = 1;

					term_conf.background_color.red = 0;
					term_conf.background_color.green = 0;
					term_conf.background_color.blue = 0;
					term_conf.background_color.alpha = 1;
				}

				i = max + 1;
			}
		}
		if(i == max)
		{
			string = g_strdup_printf(_("No section \"%s\" in configuration file\n"), config_name);
			show_message(string, MSG_ERR);

			g_free(string);
			return -1;
		}
	}

	vte_terminal_set_font(VTE_TERMINAL(display), pango_font_description_from_string(term_conf.font));

	vte_terminal_set_size (VTE_TERMINAL(display), term_conf.rows, term_conf.columns);
	vte_terminal_set_scrollback_lines (VTE_TERMINAL(display), term_conf.scrollback);
	vte_terminal_set_color_foreground (VTE_TERMINAL(display), &term_conf.foreground_color);
	vte_terminal_set_color_background (VTE_TERMINAL(display), &term_conf.background_color);
	vte_terminal_set_color_background (VTE_TERMINAL(display), &term_conf.background_color);
	vte_terminal_set_cursor_shape(VTE_TERMINAL(display), term_conf.block_cursor ? VTE_CURSOR_SHAPE_BLOCK : VTE_CURSOR_SHAPE_IBEAM);
	gtk_widget_queue_draw(display);

	return 0;
}

void hard_default_configuration(void)
{
	strcpy(config.port, DEFAULT_PORT);
	config.speed = DEFAULT_SPEED;
	config.parity = DEFAULT_PARITY;
	config.bits = DEFAULT_BITS;
	config.stops = DEFAULT_STOP;
	config.flux = DEFAULT_FLOW;
	config.delay = DEFAULT_DELAY;
	config.rs485_rts_time_before_transmit = DEFAULT_DELAY_RS485;
	config.rs485_rts_time_after_transmit = DEFAULT_DELAY_RS485;
	config.char_queue = DEFAULT_CHAR;
	config.echo = DEFAULT_ECHO;
	config.crlfauto = FALSE;
	config.timestamp = FALSE;
    config.disable_port_lock = FALSE;

	term_conf.font = g_strdup_printf(DEFAULT_FONT);

	term_conf.block_cursor = TRUE;
	term_conf.rows = 80;
	term_conf.columns = 25;
	term_conf.scrollback = DEFAULT_SCROLLBACK;
	term_conf.visual_bell = TRUE;

	set_color (&term_conf.foreground_color, 0.66, 0.66, 0.66, 1.0);
	set_color (&term_conf.background_color, 0, 0, 0, 1.0);
}

void copy_configuration(int pos)
{
	char *string = NULL;
	macro_t *macros = NULL;
	int size, i;

	string = g_strdup(config.port);
	cfgStoreValue(cfg, "port", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", config.speed);
	cfgStoreValue(cfg, "speed", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", config.bits);
	cfgStoreValue(cfg, "bits", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", config.stops);
	cfgStoreValue(cfg, "stopbits", string, CFG_INI, pos);
	g_free(string);

	switch(config.parity)
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
	cfgStoreValue(cfg, "parity", string, CFG_INI, pos);
	g_free(string);

	switch(config.flux)
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

	cfgStoreValue(cfg, "flow", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", config.delay);
	cfgStoreValue(cfg, "wait_delay", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", config.char_queue);
	cfgStoreValue(cfg, "wait_char", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", config.rs485_rts_time_before_transmit);
	cfgStoreValue(cfg, "rs485_rts_time_before_tx", string, CFG_INI, pos);
	g_free(string);
	string = g_strdup_printf("%d", config.rs485_rts_time_after_transmit);
	cfgStoreValue(cfg, "rs485_rts_time_after_tx", string, CFG_INI, pos);
	g_free(string);

	if(config.echo == FALSE)
		string = g_strdup_printf("False");
	else
		string = g_strdup_printf("True");

	cfgStoreValue(cfg, "echo", string, CFG_INI, pos);
	g_free(string);

	if(config.crlfauto == FALSE)
		string = g_strdup_printf("False");
	else
		string = g_strdup_printf("True");

	cfgStoreValue(cfg, "crlfauto", string, CFG_INI, pos);
	g_free(string);

	if(config.timestamp == FALSE)
		string = g_strdup_printf("False");
	else
		string = g_strdup_printf("True");

	cfgStoreValue(cfg, "timestamp", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup(term_conf.font);
	cfgStoreValue(cfg, "font", string, CFG_INI, pos);
	g_free(string);

	macros = get_shortcuts(&size);
	for(i = 0; i < size; i++)
	{
		string = g_strdup_printf("%s::%s", macros[i].shortcut, macros[i].action);
		cfgStoreValue(cfg, "macros", string, CFG_INI, pos);
		g_free(string);
	}

	if(term_conf.block_cursor == FALSE)
		string = g_strdup_printf("False");
	else
		string = g_strdup_printf("True");
	cfgStoreValue(cfg, "term_block_cursor", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", term_conf.rows);
	cfgStoreValue(cfg, "term_rows", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", term_conf.columns);
	cfgStoreValue(cfg, "term_columns", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", term_conf.scrollback);
	cfgStoreValue(cfg, "term_scrollback", string, CFG_INI, pos);
	g_free(string);

	if(term_conf.visual_bell == FALSE)
		string = g_strdup_printf("False");
	else
		string = g_strdup_printf("True");
	cfgStoreValue(cfg, "term_visual_bell", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%f", term_conf.foreground_color.red);
	cfgStoreValue(cfg, "term_foreground_red", string, CFG_INI, pos);
	g_free(string);
	string = g_strdup_printf("%f", term_conf.foreground_color.green);
	cfgStoreValue(cfg, "term_foreground_green", string, CFG_INI, pos);
	g_free(string);
	string = g_strdup_printf("%f", term_conf.foreground_color.blue);
	cfgStoreValue(cfg, "term_foreground_blue", string, CFG_INI, pos);
	g_free(string);
	string = g_strdup_printf("%f", term_conf.foreground_color.alpha);
	cfgStoreValue(cfg, "term_foreground_alpha", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%f", term_conf.background_color.red);
	cfgStoreValue(cfg, "term_background_red", string, CFG_INI, pos);
	g_free(string);
	string = g_strdup_printf("%f", term_conf.background_color.green);
	cfgStoreValue(cfg, "term_background_green", string, CFG_INI, pos);
	g_free(string);
	string = g_strdup_printf("%f", term_conf.background_color.blue);
	cfgStoreValue(cfg, "term_background_blue", string, CFG_INI, pos);
	g_free(string);
	string = g_strdup_printf("%f", term_conf.background_color.alpha);
	cfgStoreValue(cfg, "term_background_alpha", string, CFG_INI, pos);
	g_free(string);
}

void set_color(GdkRGBA *color, float R, float G, float B, float A)
{
	color->red = R;
	color->green = G;
	color->blue = B;
	color->alpha = A;
}
