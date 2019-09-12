/***********************************************************************/
/* interface.h                                                           */
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

#define MSG_WRN 0
#define MSG_ERR 1

#define ASCII_VIEW 0
#define HEXADECIMAL_VIEW 1

void create_main_window(void);
void Set_status_message(gchar *);
void put_text(gchar *, guint);
void put_hexadecimal(gchar *, guint);
void Set_local_echo(gboolean);
void show_message(gchar *, gint);
void clear_display(void);
void set_view(guint);
gint send_serial(gchar *, gint);
void Put_temp_message(const gchar *, gint);
void Set_window_title(gchar *msg);

void toggle_logging_pause_resume(gboolean currentlyLogging);
void toggle_logging_sensitivity(gboolean currentlyLogging);

void catch_signals();

extern GtkWidget *Fenetre;
extern GtkWidget *StatusBar;
extern guint id;
extern GtkWidget *Text;
extern GtkAccelGroup *shortcuts;

#endif
