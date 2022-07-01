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

typedef struct
{
	char *shortcut;
	char *action;
	GClosure *closure;
}
macro_t;

//void config_macros(GtkAction *action, gpointer data);
void remove_shortcuts(void);
void add_shortcuts(void);
void create_shortcuts(macro_t *, gint);
macro_t *get_shortcuts(gint *);

void convert_string_to_macros (char **);
int convert_macros_to_string (char **);

int macro_count ();

extern macro_t *macros;

#endif
