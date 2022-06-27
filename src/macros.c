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

