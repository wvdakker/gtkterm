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

void send_raw_file(GSimpleAction *action, GVariant *param, gpointer data);
void send_text_file(GSimpleAction *action, GVariant *param, gpointer data);
void save_raw_file(GSimpleAction *action, GVariant *param, gpointer data);
void save_ascii_file(GSimpleAction *action, GVariant *param, gpointer data);
void files_cleanup(void);

extern gchar *fic_defaut;

#endif
