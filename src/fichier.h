/***********************************************************************/
/* fichier.h                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                      julien@jls-info.com                            */                      
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Raw / text file transfer management                            */
/*      - Header file -                                                */
/*                                                                     */
/***********************************************************************/

#ifndef FICHIER_H_
#define FICHIER_H_

gint fichier(GtkWidget *widget, guint param);
void add_input(void);

extern gboolean waiting_for_char;
extern gchar *fic_defaut;


#endif
