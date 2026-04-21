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

#include <gtk/gtk.h>
#include <vte/vte.h>

#define MSG_WRN 0
#define MSG_ERR 1

#define ASCII_VIEW 0
#define HEXADECIMAL_VIEW 1

void create_main_window(GtkApplication *app);
void initialize_hexadecimal_display(void);
void update_port_status(void);
void put_text(const gchar *, gsize);
void put_hexadecimal(const gchar *, gsize);
void Set_local_echo(gboolean);
void Set_shortcuts_disabled(gboolean);
void show_message(gint type_msg, const gchar *message);
void show_messagef(gint type_msg, const gchar *fmt, ...) G_GNUC_PRINTF(2, 3);
void clear_display(void);
void set_view(guint);
void Set_crlfauto(gboolean crlfauto);
void Set_autoreconnect_enabled(gboolean autoreconnect_enabled);
void Set_esc_clear_screen(gboolean esc_clear_screen);
void Set_timestamp(gboolean timestamp);
gssize send_serial(const gchar *, gsize);
void Put_temp_message(const gchar *, guint);
void Set_window_title(const gchar *msg);
void interface_close_port(void);
void interface_open_port(void);
void interface_apply_term_config(void);
void interface_cleanup(void);
void show_control_signals(int stat);
gboolean control_signals_read(gpointer user_data);

void toggle_logging_pause_resume(gboolean currentlyLogging);
void toggle_logging_sensitivity(gboolean currentlyLogging);

extern GtkWidget *Fenetre;
extern GtkWidget *StatusBar;
extern VteTerminal *display;

#endif

