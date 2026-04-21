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
	gchar          *shortcut;
	gchar          *action;
	gchar          *expanded;
	gsize           expanded_len;  /* cached strlen(expanded) */
	guint           keyval;
	GdkModifierType mods;
}
macro_t;

void Config_macros(GSimpleAction *action, GVariant *param, gpointer data);
void remove_shortcuts(void);
void create_shortcuts(macro_t *, gsize);
macro_t *get_shortcuts(gsize *);
void install_macro_shortcut_controller(GtkWidget *win);
void set_macros_shortcuts_enabled(gboolean enabled);

#endif

