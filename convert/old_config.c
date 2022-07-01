#include <stdio.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <pango/pango-font.h>
#include "parsecfg.h"
#include "macros.h"
#include "interface.h"
#include "serial.h"
#include "term_config.h"
#include "resource_file.h"

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

int load_old_configuration_from_file(char *config_name)
{
	int max, i, j, k, size;
	char *string = NULL;
	char *str;
	macro_t *macros = NULL;
	cfgList *t;

	max = cfgParse(g_file_get_path(config_file), cfg, CFG_INI);

	if(max == -1)
	{
		show_message(_("Cannot read configuration file!\nIf no previous file is used this conversion can be ommited.\n"), MSG_ERR);
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
					strcpy(port_conf.port, port[i]);
				if(speed[i] != 0)
					port_conf.speed = speed[i];
				if(bits[i] != 0)
					port_conf.bits = bits[i];
				if(stopbits[i] != 0)
					port_conf.stops = stopbits[i];
				if(parity[i] != NULL)
				{
					if(!g_ascii_strcasecmp(parity[i], "none"))
						port_conf.parity = 0;
					else if(!g_ascii_strcasecmp(parity[i], "odd"))
						port_conf.parity = 1;
					else if(!g_ascii_strcasecmp(parity[i], "even"))
						port_conf.parity = 2;
				}
				if(flow[i] != NULL)
				{
					if(!g_ascii_strcasecmp(flow[i], "none"))
						port_conf.flow_control = 0;
					else if(!g_ascii_strcasecmp(flow[i], "xon"))
						port_conf.flow_control = 1;
					else if(!g_ascii_strcasecmp(flow[i], "rts"))
						port_conf.flow_control = 2;
					else if(!g_ascii_strcasecmp(flow[i], "rs485"))
						port_conf.flow_control = 3;
				}

				term_conf.delay = wait_delay[i];

				if(wait_char[i] != 0)
					term_conf.char_queue = (signed char)wait_char[i];
				else
					term_conf.char_queue = -1;

				port_conf.rs485_rts_time_before_transmit = rts_time_before_tx[i];
				port_conf.rs485_rts_time_after_transmit = rts_time_after_tx[i];

				if(echo[i] != -1)
					term_conf.echo = (gboolean)echo[i];
				else
					term_conf.echo = FALSE;

				if(crlfauto[i] != -1)
					term_conf.crlfauto = (gboolean)crlfauto[i];
				else
					term_conf.crlfauto = FALSE;

				if(timestamp[i] != -1)
					term_conf.timestamp = (gboolean)timestamp[i];
				else
					term_conf.timestamp = FALSE;

				if (term_conf.font != NULL)
					g_clear_pointer (&term_conf.font, pango_font_description_free);
				term_conf.font = pango_font_description_from_string(font[i]);

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
				create_shortcuts(macros, size);
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

	return 0;
}