/***********************************************************************/
/* terminal.h                                                       */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Handles all VTE in/output to/from serial port                  */
/*      - Header file -                                                */
/*                                                                     */
/***********************************************************************/
#ifndef TERMINAL_H
#define TERMINAL_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

#include "gtkterm.h"

typedef struct
{
	bool block_cursor;
	bool show_cursor;
	char char_queue;            // character in queue
	bool echo;               	// echo local
	bool crlfauto;           	// line feed auto
	bool timestamp;
	int delay;                  // end of char delay: in ms
	int rows;
	int columns;
	int scrollback;
	bool visual_bell;
	GdkRGBA foreground_color;
	GdkRGBA background_color;
	PangoFontDescription *font;

} term_config_t;

G_BEGIN_DECLS

#define GTKTERM_TYPE_TERMINAL gtkterm_terminal_get_type()
G_DECLARE_FINAL_TYPE (GtkTermTerminal, gtkterm_terminal, GTKTERM, TERMINAL, VteTerminal)

GtkTermTerminal *gtkterm_terminal_new (char *, GtkTerm *, GtkTermWindow *);

G_END_DECLS

#endif // TERMINAL_H