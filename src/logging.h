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

void logging_start(GSimpleAction *action, GVariant *param, gpointer data);
void logging_pause_resume(GSimpleAction *action, GVariant *param, gpointer data);
void logging_stop(GSimpleAction *action, GVariant *param, gpointer data);
void logging_clear(GSimpleAction *action, GVariant *param, gpointer data);
void log_chars(const gchar *chars, guint size);
void logging_cleanup(void);

#endif /* LOGGING_H_ */
