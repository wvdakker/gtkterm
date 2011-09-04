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

void logging_start(GtkAction *action, gpointer data);
void logging_pause_resume(void);
void logging_stop(void);
void logging_clear(void);
void log_chars(gchar *chars, guint size);

#endif /* LOGGING_H_ */
