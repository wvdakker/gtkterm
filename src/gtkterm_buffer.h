/************************************************************************/
/* gtkterm_buffer.h                                                   	*/
/* ----------------                                                    	*/
/*           GTKTerm Software                                          	*/
/*                      (c) Julien Schmitt                             	*/
/*                                                                     	*/
/* ------------------------------------------------------------------- 	*/
/*                                                                     	*/
/*   Purpose                                                           	*/
/*      Include file for GtkTermBuffer                                	*/
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

#ifndef GTMTERM_BUFFER_H
#define GTKTERM_BUFFER_H

/**
 * @brief  Enum buffer_error id.
 * 
 * Many of the gtk_buffer functions return
 * an error id.
 */
typedef enum {
	GTKTERM_BUFFER_SUCCESS,
	GTKTERM_BUFFER_NOT_INITALIZED,
	GTKTERM_BUFFER_OVERFLOW,
	GTKTERM_BUFFER_LAST

} GtkTermBufferState;

G_BEGIN_DECLS

#define GTKTERM_TYPE_BUFFER gtkterm_buffer_get_type ()
G_DECLARE_FINAL_TYPE (GtkTermBuffer, gtkterm_buffer, GTKTERM, BUFFER, GObject)
typedef struct _GtkTermBuffer GtkTermBuffer;

GtkTermBuffer *gtkterm_buffer_new (GtkTermSerialPort *, GtkTermTerminal *, term_config_t *);

G_END_DECLS

GtkTermBufferState gtkterm_buffer_get_status (GtkTermBuffer *);
GError *gtkterm_buffer_get_error (GtkTermBuffer*);

#endif // GTKTERM_BUFFER_H