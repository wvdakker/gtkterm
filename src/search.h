/***********************************************************************/
/* search.h                                                            */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Search text from the VTE                                       */
/*   Written by Tomi Lähteenmäki - lihis@lihis.net                     */
/*                                                                     */
/***********************************************************************/

#ifndef FIND_H
#define FIND_H

#include <vte/vte.h>

GtkWidget *search_bar_new(GtkWindow *parent, VteTerminal *terminal);
void search_bar_show(GtkWidget *search_box);
void search_bar_hide(GtkWidget *search_box);

#endif
