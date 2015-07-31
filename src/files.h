/***********************************************************************/
/* files.h                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
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

void send_raw_file(GtkAction *action, gpointer data);
void save_raw_file(GtkAction *action, gpointer data);
void add_input(void);

extern gboolean waiting_for_char;
extern gchar *fic_defaut;


#endif
