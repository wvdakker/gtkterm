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

#ifndef FILES_H_
#define FILES_H_

void send_raw_file (GAction *action, gpointer data);
void save_raw_file (GAction *action, gpointer data);
void add_input(void);

extern gboolean waiting_for_char;
extern char *default_filename;

#endif
