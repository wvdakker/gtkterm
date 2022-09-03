/************************************************************************/
/* gtkterm_terminal.h                                               	*/
/* ------------------                                               	*/
/*           GTKTerm Software                                          	*/
/*                      (c) Julien Schmitt                             	*/
/*                                                                     	*/
/* ------------------------------------------------------------------- 	*/
/*                                                                     	*/
/*   Purpose                                                           	*/
/*      Include file for GtkTermTerminal	                 			*/
/*                                                                     	*/
/* This GtkTerm is free software: you can redistribute it and/or modify	*/ 
/* it under the terms of the GNU  General Public License as published  	*/
/* by the Free Software Foundation, either version 3 of the License,   	*/
/* or (at your option) any later version.							   	*/
/*																	   	*/
/* GtkTerm is distributed in the hope that it will be useful, but	   	*/
/* WITHOUT ANY WARRANTY; without even the implied warranty of 		   	*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 			   	*/
/* See the GNU General Public License for more details.					*/
/*																		*/
/* You should have received a copy of the GNU General Public License 	*/
/* along with GtkTerm If not, see <https://www.gnu.org/licenses/>. 		*/
/*                                                                     	*/
/************************************************************************/

#ifndef GTKTERM_TERMINAL_H
#define GTKTERM_TERMINAL_H

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
	bool auto_lf;           	/** auto line feed				*/
	bool auto_cr;           	/** auto return				*/
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
typedef struct _GtkTermTerminal GtkTermTerminal;

GtkTermTerminal *gtkterm_terminal_new (char *, GtkTerm *, GtkTermWindow *);

G_END_DECLS

#endif // GTKTERM_TERMINAL_H