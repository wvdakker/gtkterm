/***********************************************************************/
/* interface.h                                                         */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Functions for the management of the GUI for the main window    */
/*      - Header file -                                                */
/*                                                                     */
/***********************************************************************/

#ifndef WIDGETS_H_
#define WIDGETS_H_

#define MSG_WRN          0
#define MSG_ERR          1

#define ASCII_VIEW       0
#define HEXADECIMAL_VIEW 1

extern GtkWidget *Text;
extern GtkWidget *display;          // Serial terminal (vte)

void show_message(char *, int);

#endif
