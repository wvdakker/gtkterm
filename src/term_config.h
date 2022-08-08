/***********************************************************************/
/* term_config.h                                                       */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Configuration of the serial port                               */
/*      - Header file -                                                */
/*                                                                     */
/***********************************************************************/

#ifndef TERM_CONFIG_H_
#define TERM_CONFIG_H_

#define DEFAULT_FONT "Monospace 12"
#define DEFAULT_SCROLLBACK 10000

#define DEFAULT_DELAY 		0
#define DEFAULT_CHAR 		-1
#define DEFAULT_DELAY_RS485 30
#define DEFAULT_ECHO 		"false"
#define DEFAULT_VISUAL_BELL "false"

typedef struct
{
	bool block_cursor;
	bool show_cursor;
	char char_queue;             // character in queue
	bool echo;               // echo local
	bool crlfauto;           // line feed auto
	bool timestamp;
	int delay;                  // end of char delay: in ms
	int rows;
	int columns;
	int scrollback;
	bool visual_bell;
	GdkRGBA foreground_color;
	GdkRGBA background_color;
	PangoFontDescription *font;
	char *active_section;

	char *default_filename;

} display_config_t;

// configuration for the terminal window
extern display_config_t term_conf;

#endif
