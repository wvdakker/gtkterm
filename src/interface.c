/***********************************************************************/
/* widgets.c                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Functions for the management of the GUI for the main window    */
/*                                                                     */
/*   ChangeLog                                                         */
/*   (All changes by Julien Schmitt except when explicitly written)    */
/*                                                                     */
/*       - 1.01  : The put_hexadecimal partly function rewritten.      */
/*                 The vte_terminal_get_cursor_position function does  */
/*                 not return always the actual column.                */
/*                 Now it uses an internal column-index (virt_col_pos).*/
/*                 (Willem van den Akker)                              */
/*      - 0.99.7 : Changed keyboard shortcuts to <ctrl><shift>         */
/*	            (Ken Peek)                                         */
/*      - 0.99.6 : Added scrollbar and copy/paste (Zach Davis)         */
/*                                                                     */
/*      - 0.99.5 : Make package buildable on pure *BSD by changing the */
/*                 include to asm/termios.h by sys/ttycom.h            */
/*                 Print message without converting it into the locale */
/*                 in show_message()                                   */
/*                 Set backspace key binding to backspace so that the  */
/*                 backspace works. It would even be nicer if the      */
/*                 behaviour of this key could be configured !         */
/*      - 0.99.4 : - Sebastien Bacher -                                */
/*                 Added functions for CR LF auto mode                 */
/*                 Fixed put_text() to have \r\n for the VTE Widget    */
/*                 Rewritten put_hexadecimal() function                */
/*                 - Julien -                                          */
/*                 Modified send_serial to return the actual number of */
/*                 bytes written, and also only display exactly what   */
/*                 is written                                          */
/*      - 0.99.3 : Modified to use a VTE terminal                      */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.99.0 : \b byte now handled correctly by the ascii widget   */
/*                 SUPPR (0x7F) also prints correctly                  */
/*                 adapted for macros                                  */
/*                 modified "about" dialog                             */
/*      - 0.98.6 : fixed possible buffer overrun in hex send           */
/*                 new "Send break" option                             */
/*      - 0.98.5 : icons in the menu                                   */
/*                 bug fixed with local echo and hexadecimal           */
/*                 modified hexadecimal send separator, and bug fixed  */
/*      - 0.98.4 : new hexadecimal display / send                      */
/*      - 0.98.3 : put_text() modified to fit with 0x0D 0x0A           */
/*      - 0.98.2 : added local echo by Julien                          */
/*      - 0.98 : file creation by Julien                               */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#if defined (__linux__)
#  include <asm/termios.h>       /* For control signals */
#endif
#if defined (__FreeBSD__) || defined (__FreeBSD_kernel__) \
     || defined (__NetBSD__) || defined (__NetBSD_kernel__) \
     || defined (__OpenBSD__) || defined (__OpenBSD_kernel__)
#  include <sys/ttycom.h>        /* For control signals */
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vte/vte.h>

#include <config.h>
#include <glib/gi18n.h>

#include "interface.h"

bool timestamp_on = 0;
extern struct configuration_port config;
int virt_col_pos = 0;

void show_message(char *message, int type_msg)
{
//	GtkWidget *message_dialog;

//	if(type_msg == MSG_ERR)
//	{
//        message_dialog = gtk_message_dialog_new(GTK_WINDOW(message_dialog),
//		                                     GTK_DIALOG_DESTROY_WITH_PARENT,
//		                                     GTK_MESSAGE_ERROR,
//		                                     GTK_BUTTONS_OK,
//		                                     message, NULL);
//	}
//	else if(type_msg == MSG_WRN)
//	{
//		message_dialog = gtk_message_dialog_new(GTK_WINDOW(message_dialog),
//		                                     GTK_DIALOG_DESTROY_WITH_PARENT,
//		                                     GTK_MESSAGE_WARNING,
//		                                     GTK_BUTTONS_OK,
//		                                     message, NULL);
//	}
//	else
//		return;

//	gtk_dialog_run(GTK_DIALOG(message_dialog));

//	gtk_widget_destroy(message_dialog);
}