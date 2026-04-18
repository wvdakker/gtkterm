/***********************************************************************/
/* search.c                                                            */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Search text from the VTE                                       */
/*   Written by Tomi Lähteenmäki - lihis@lihis.net                     */
/*   Ported to GTK4                                                    */
/*                                                                     */
/***********************************************************************/

#include "search.h"
#include <glib/gi18n.h>

#define PCRE2_CODE_UNIT_WIDTH 0
#include <pcre2.h>

static GtkWindow *parentWindow;
static VteTerminal *term;
static GtkSearchBar *searchBar;
static GtkWidget *prevButton;
static GtkWidget *nextButton;
static VteRegex *regex;
static GtkEditable *entry;

typedef enum
{
	FIND_PREVIOUS,
	FIND_NEXT
} FindDirection;

/* Forward declarations */
void search_direction_cb(GtkButton *btn, gpointer data G_GNUC_UNUSED);

static void entry_changed_callback(void)
{
	gboolean sensitive = FALSE;

	if (regex != NULL)
	{
		vte_regex_unref(regex);
		regex = NULL;
	}

	if (gtk_editable_get_text(entry)[0] != '\0')
		sensitive = TRUE;

	gtk_widget_set_sensitive(prevButton, sensitive);
	gtk_widget_set_sensitive(nextButton, sensitive);
}

static void search_callback(GtkWidget *widget G_GNUC_UNUSED, gpointer data)
{
	FindDirection direction = (FindDirection)GPOINTER_TO_UINT(data);

	if (regex == NULL)
	{
		const gchar *pattern = gtk_editable_get_text(entry);
		GError *error = NULL;
		regex = vte_regex_new_for_search(pattern,
		                                 (gssize)strlen(pattern),
		                                 PCRE2_MULTILINE | PCRE2_CASELESS,
		                                 &error);
		if (regex == NULL)
		{
			GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", error->message);
			gtk_alert_dialog_set_modal(dialog, TRUE);
			gtk_alert_dialog_show(dialog, GTK_WINDOW(parentWindow));
			g_object_unref(dialog);
			g_error_free(error);
			return;
		}

		if (term)
			vte_terminal_search_set_regex(term, regex, 0);
	}

	if (direction == FIND_PREVIOUS)
		vte_terminal_search_find_previous(term);
	else
		vte_terminal_search_find_next(term);
}

static gboolean entry_key_press_cb(GtkEventControllerKey *ctrl G_GNUC_UNUSED,
                                   guint keyval, guint keycode G_GNUC_UNUSED,
                                   GdkModifierType state, gpointer user_data G_GNUC_UNUSED)
{
	guint mask = gtk_accelerator_get_default_mod_mask();
	gboolean handled = FALSE;

	if ((state & mask) == 0)
	{
		if (keyval == GDK_KEY_Escape)
		{
			search_bar_hide(GTK_WIDGET(searchBar));
			handled = TRUE;
		}
	}
	else if ((state & mask) == GDK_SHIFT_MASK &&
	         (keyval == GDK_KEY_Return ||
	          keyval == GDK_KEY_KP_Enter ||
	          keyval == GDK_KEY_ISO_Enter))
	{
		search_callback(NULL, GUINT_TO_POINTER(FIND_PREVIOUS));
		handled = TRUE;
	}

	return handled;
}

GtkWidget *search_bar_new(GtkWindow *parent, VteTerminal *terminal)
{
	GtkBuilderCScope *scope;
	GtkBuilder *builder;

	parentWindow = parent;
	term = terminal;
	regex = NULL;
	if (term)
		vte_terminal_search_set_wrap_around(term, TRUE);

	scope = GTK_BUILDER_CSCOPE(gtk_builder_cscope_new());
	gtk_builder_cscope_add_callback_symbols(scope,
	    "entry_changed_callback", G_CALLBACK(entry_changed_callback),
	    "search_direction_cb",    G_CALLBACK(search_direction_cb),
	    "entry_key_press_cb",     G_CALLBACK(entry_key_press_cb),
	    NULL);
	builder = gtk_builder_new();
	gtk_builder_set_scope(builder, GTK_BUILDER_SCOPE(scope));
	g_object_unref(scope);
	gtk_builder_add_from_resource(builder, "/org/gtk/gtkterm/search_bar.ui", NULL);

	searchBar   = GTK_SEARCH_BAR(gtk_builder_get_object(builder, "search_bar"));
	entry       = GTK_EDITABLE(gtk_builder_get_object(builder, "search_entry"));
	prevButton  = GTK_WIDGET(gtk_builder_get_object(builder, "search_prev_button"));
	nextButton  = GTK_WIDGET(gtk_builder_get_object(builder, "search_next_button"));

	gtk_search_bar_connect_entry(searchBar, entry);

	g_object_ref(searchBar);
	g_object_unref(builder);

	return GTK_WIDGET(searchBar);
}

void search_direction_cb(GtkButton *btn, gpointer data G_GNUC_UNUSED)
{
	const char *id = gtk_buildable_get_buildable_id(GTK_BUILDABLE(btn));
	FindDirection dir = g_str_has_suffix(id, "prev_button") ? FIND_PREVIOUS : FIND_NEXT;
	search_callback(NULL, GUINT_TO_POINTER(dir));
}

void search_bar_show(GtkWidget *self)
{
	gtk_widget_set_visible(self, TRUE);
	gtk_search_bar_set_search_mode(searchBar, TRUE);
	gtk_widget_grab_focus(GTK_WIDGET(entry));
}

void search_bar_hide(GtkWidget *self)
{
	gtk_widget_set_visible(self, FALSE);
	if (term)
		vte_terminal_search_set_regex(term, NULL, 0);
	gtk_search_bar_set_search_mode(searchBar, FALSE);

	if (regex != NULL)
	{
		vte_regex_unref(regex);
		regex = NULL;
	}
}
