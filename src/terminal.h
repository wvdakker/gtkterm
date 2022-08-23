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

/**
 * @brief The typedef for the terminal configuration.
 *
 */
typedef struct {
	
	bool block_cursor;			/** Show a block shape cursor	*/
	bool show_cursor;			/** Show cursor in window. \todo This is not possible, so remove? */
	char char_queue;            /** character in queue			*/
	bool echo;               	/** local echo 					*/
	bool crlfauto;           	/** auto line feed				*/
	bool timestamp;				/** Show timestamp in output	*/
	int delay;                  /** end of char delay: in ms	*/
	int rows;					/** Number of rows in terminal  */
	int columns;				/** Number of cols in terminal  */
	int scrollback;				/** Number of scrollback lines  */
	bool visual_bell;			/**	Visual bell					*/
	GdkRGBA foreground_color;	/** Terminal Background color	*/
	GdkRGBA background_color;	/** Terminal Foreground color   */
	PangoFontDescription *font;	/** Terminal Font				*/

} term_config_t;

G_BEGIN_DECLS

#define GTKTERM_TYPE_TERMINAL gtkterm_terminal_get_type()
G_DECLARE_FINAL_TYPE (GtkTermTerminal, gtkterm_terminal, GTKTERM, TERMINAL, VteTerminal)

GtkTermTerminal *gtkterm_terminal_new (char *, GtkTerm *, GtkTermWindow *);

G_END_DECLS

#endif // TERMINAL_H