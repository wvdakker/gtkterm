/***********************************************************************
 * macros.c                                                         
 * --------                                       
 *           GTKTerm Software                        
 *                      (c) Julien Schmitt         
 *                                              
 * -------------------------------------------------------------------
 *                                            
 *   \brief Purpose                                        
 *      	Functions for the management of the macros  
 *                                              
 *   ChangeLog
 *		- 2.0	 : Add conversion functions to/from string arrays                       
 *      - 0.99.2 : Internationalization                 
 *      - 0.99.0 : file creation by Julien
 *                                                         
 ***********************************************************************/

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

//! TODO: Migrate to GObject

enum
{
	COLUMN_SHORTCUT,
	COLUMN_ACTION,
	NUM_COLUMNS
};

macro_t *macros = NULL;

// Number of macro's
int nr_of_macros = 0;

int macro_count () {
	return (nr_of_macros);
}

//! Convert the array of strings to macros
void convert_string_to_macros (char **string_list, int size) {
	char **strptr = string_list;
	
	// Remove existing macro's
	remove_shortcuts ();

	if (macros)
		g_free(macros);

	// Allocate memory size. To be sure we use the
	// size of the array. We only need half.
	macros = g_malloc(size * sizeof(macro_t));
	nr_of_macros = 0;

	// Copy into macro untill end of array
	while  (*strptr) {
		macros[nr_of_macros].shortcut = g_strdup(*strptr++);
		macros[nr_of_macros++].action = g_strdup(*strptr++);
	}
}

//! Convert the in memory macros to an array of strings
//! for storage in file
int convert_macros_to_string (char **string_list) {
	char **strptr = string_list;

	for (int i = 0; i < nr_of_macros; i++) {
		*strptr++ = macros[i].shortcut;
		*strptr++ = macros[i].action;
	}

	//! Must be NULL terminated
	*strptr++ = NULL;

	//! Number of strings is 2x the macros (shortcut and action)
	return (nr_of_macros * 2);
}

static void macros_destroy(void)
{
	int i = 0;

	if(macros == NULL)
		return;

	//! Free all macro-member memory
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

	//! Clean up all macros
	macros_destroy();
}