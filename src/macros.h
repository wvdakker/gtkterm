/***********************************************************************/
/* macros.h                                                            */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Functions for the management of the macros                     */
/*      - Header file -                                                */
/*                                                                     */
/***********************************************************************/

#ifndef MACROS_H_
#define MACROS_H_

#include <gio/gio.h>

typedef struct
{
	gchar *shortcut;
	gchar *action;
}
macro_t;

void Config_macros(GSimpleAction *action, GVariant *param, gpointer data);
void remove_shortcuts(void);
void create_shortcuts(macro_t *, gint);
macro_t *get_shortcuts(gint *);
void install_macro_shortcut_controller(GtkWidget *main_window);
void set_macros_shortcuts_enabled(gboolean enabled);

#endif

