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

static int nr_of_macros = 0;

int macro_count () {
	return (nr_of_macros);
}

void convert_string_to_macros (char **string_list, int size) {
	char **strptr = string_list;
	
	// Remove existing macro's
	remove_shortcuts ();

	if (macros)
		g_free(macros);

	macros = g_malloc(size * sizeof(macro_t));
	nr_of_macros = 0;

	while  (*strptr) {
		macros[nr_of_macros].shortcut = g_strdup(*strptr++);
		macros[nr_of_macros++].action = g_strdup(*strptr++);
	}
}

int convert_macros_to_string (char **string_list) {
	char **strptr = string_list;

	for (int i = 0; i < nr_of_macros; i++) {
		*strptr++ = macros[i].shortcut;
		*strptr++ = macros[i].action;
	}

	*strptr++ = NULL;

	return (nr_of_macros * 2);
}

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

		nr_of_macros = size;
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