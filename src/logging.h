/***********************************************************************/
/* logging.h                                                           */
/* ---------                                                           */
/*                           GTKTerm Software                          */
/*                                 (c)                                 */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Log all data that GTKTerm sees to a file                       */
/*      - Header File -                                                */
/***********************************************************************/

#ifndef LOGGING_H_
#define LOGGING_H_


gint logging_start(GtkWidget *);
void logging_pause(void);
void logging_stop(void);
void log_chars(gchar *chars, unsigned int size);

#endif /* LOGGING_H_ */
