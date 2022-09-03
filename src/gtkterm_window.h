/************************************************************************/
/* gtkterm_window.h                                                   	*/
/* ----------------                                                    	*/
/*           GTKTerm Software                                          	*/
/*                      (c) Julien Schmitt                             	*/
/*                                                                     	*/
/* ------------------------------------------------------------------- 	*/
/*                                                                     	*/
/*   Purpose                                                           	*/
/*      Include file for GtkTermWindow                       			*/
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

#ifndef GTKTERM_WINDOW_H
#define GTKTERM_WINDOW_H

#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>


G_BEGIN_DECLS

#define GTKTERM_TYPE_GTKTERM_WINDOW gtkterm_window_get_type()
G_DECLARE_FINAL_TYPE (GtkTermWindow, gtkterm_window, GTKTERM, WINDOW, GtkApplicationWindow)
typedef struct _GtkTermWindow GtkTermWindow;

void create_window (GApplication *, GtkTermWindow *);
void gtkterm_show_infobar (GtkTermWindow *, char *, int);

G_END_DECLS

#endif // GTKTERM_WINDOW_H