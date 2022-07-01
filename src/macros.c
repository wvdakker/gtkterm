/***********************************************************************/
/* macros.c                                                            */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Functions for the management of the macros                     */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.99.0 : file creation by Julien                             */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "interface.h"
#include "macros.h"

#include <config.h>
#include <glib/gi18n.h>

enum
{
	COLUMN_SHORTCUT,
	COLUMN_ACTION,
	NUM_COLUMNS
};

macro_t *macros = NULL;

static void macros_destroy(void)
{
	int i = 0;

	if(macros == NULL)
		return;

	while(macros[i].shortcut != NULL)
	{
		g_free(macros[i].shortcut);
		g_free(macros[i].action);
		/*
		    g_closure_unref(macros[i].closure);
		*/
		i++;
	}

	g_free(macros);
	macros = NULL;
}

macro_t *get_shortcuts(int *size)
{
	int i = 0;

	if(macros != NULL)
	{
		while(macros[i].shortcut != NULL)
			i++;
	}
	*size = i;
	
	return macros;
}

void create_shortcuts(macro_t *macro, int size)
{
	macros = g_malloc((size + 1) * sizeof(macro_t));
	if(macros != NULL)
	{
		memcpy(macros, macro, size * sizeof(macro_t));
		macros[size].shortcut = NULL;
		macros[size].action = NULL;
	}
	else
		perror("malloc");
}

void remove_shortcuts(void)
{
	int i = 0;

	if(macros == NULL)
		return;

	while(macros[i].shortcut != NULL)
	{
//		gtk_accel_group_disconnect(shortcuts, macros[i].closure);
		i++;
	}

	macros_destroy();
}