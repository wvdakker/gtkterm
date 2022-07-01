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

int load_old_configuration_from_file(int section_nr)
{
	int j, k, size;
	char *str;
	macro_t *macros = NULL;
	cfgList *t;

	hard_default_configuration();

	if(port[section_nr] != NULL)
		strcpy(port_conf.port, port[section_nr]);
	if(speed[section_nr] != 0)
		port_conf.speed = speed[section_nr];
	if(bits[section_nr] != 0)
		port_conf.bits = bits[section_nr];
	if(stopbits[section_nr] != 0)
		port_conf.stops = stopbits[section_nr];
	if(parity[section_nr] != NULL)
	{
		if(!g_ascii_strcasecmp(parity[section_nr], "none"))
			port_conf.parity = 0;
		else if(!g_ascii_strcasecmp(parity[section_nr], "odd"))
			port_conf.parity = 1;
		else if(!g_ascii_strcasecmp(parity[section_nr], "even"))
			port_conf.parity = 2;
	}
	if(flow[section_nr] != NULL)
	{
		if(!g_ascii_strcasecmp(flow[section_nr], "none"))
			port_conf.flow_control = 0;
		else if(!g_ascii_strcasecmp(flow[section_nr], "xon"))
			port_conf.flow_control = 1;
		else if(!g_ascii_strcasecmp(flow[section_nr], "rts"))
			port_conf.flow_control = 2;
		else if(!g_ascii_strcasecmp(flow[section_nr], "rs485"))
			port_conf.flow_control = 3;
	}

	term_conf.delay = wait_delay[section_nr];

	if(wait_char[section_nr] != 0)
		term_conf.char_queue = (signed char)wait_char[section_nr];
	else
		term_conf.char_queue = -1;

	port_conf.rs485_rts_time_before_transmit = rts_time_before_tx[section_nr];
	port_conf.rs485_rts_time_after_transmit = rts_time_after_tx[section_nr];

	if(echo[section_nr] != -1)
		term_conf.echo = (gboolean)echo[section_nr];
	else
		term_conf.echo = FALSE;

	if(crlfauto[section_nr] != -1)
		term_conf.crlfauto = (gboolean)crlfauto[section_nr];
	else
		term_conf.crlfauto = FALSE;

	if(timestamp[section_nr] != -1)
		term_conf.timestamp = (gboolean)timestamp[section_nr];
	else
		term_conf.timestamp = FALSE;

	if (term_conf.font != NULL)
		g_clear_pointer (&term_conf.font, pango_font_description_free);
	term_conf.font = pango_font_description_from_string(font[section_nr]);

	// remove old macro's and free memory
	remove_shortcuts ();

	t = macro_list[section_nr];
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
		t = macro_list[section_nr];
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

	create_shortcuts(macros, size);
	g_free(macros);

	if(block_cursor[section_nr] != -1)
		term_conf.block_cursor = (gboolean)block_cursor[section_nr];
	else
		term_conf.block_cursor = TRUE;

	if(rows[section_nr] != 0)
		term_conf.rows = rows[section_nr];

	if(columns[section_nr] != 0)
		term_conf.columns = columns[section_nr];

	if(scrollback[section_nr] != 0)
		term_conf.scrollback = scrollback[section_nr];

	if(visual_bell[section_nr] != -1)
		term_conf.visual_bell = (gboolean)visual_bell[section_nr];
	else
		term_conf.visual_bell = FALSE;

	term_conf.foreground_color.red = foreground_red[section_nr];
	term_conf.foreground_color.green = foreground_green[section_nr];
	term_conf.foreground_color.blue = foreground_blue[section_nr];
	term_conf.foreground_color.alpha = foreground_alpha[section_nr];

	term_conf.background_color.red = background_red[section_nr];
	term_conf.background_color.green = background_green[section_nr];
	term_conf.background_color.blue = background_blue[section_nr];
	term_conf.background_color.alpha = background_alpha[section_nr];

	return 0;
}