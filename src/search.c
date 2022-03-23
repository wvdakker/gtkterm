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
/*                                                                     */
/***********************************************************************/

#include "search.h"
#include <glib/gi18n.h>

#define PCRE2_CODE_UNIT_WIDTH 0
#include <pcre2.h>

static GtkWindow *parentWindow;
static VteTerminal *term;
static GtkWidget *box;
static GtkWidget *searchBar;
static GtkWidget *prevImage;
static GtkWidget *prevButton;
static GtkWidget *nextImage;
static GtkWidget *nextButton;
static VteRegex *regex;
static GtkWidget* entry;

typedef enum
{
	FIND_PREVIOUS,
	FIND_NEXT
} FindDirection;

void entry_changed_callback()
{
	gboolean sensitive = FALSE;

	if(regex != NULL)
	{
		vte_regex_unref(regex);
		regex = NULL;
	}

	if(gtk_entry_get_text_length(GTK_ENTRY(entry)))
		sensitive = TRUE;

	gtk_widget_set_sensitive(prevButton, sensitive);
	gtk_widget_set_sensitive(nextButton, sensitive);
}

void search_callback(GtkWidget *widget, gpointer data)
{
	(void)widget;
	FindDirection direction = (FindDirection)GPOINTER_TO_UINT(data);

	if(regex == NULL)
	{
		const gchar *pattern = gtk_entry_get_text(GTK_ENTRY(entry));
		GError *error = NULL;
		regex = vte_regex_new_for_search(pattern,
						 strlen(pattern),
						 PCRE2_MULTILINE,
						 &error);
		if(regex == NULL)
		{
			GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
			GtkWidget *dialog = gtk_message_dialog_new(parentWindow,
									 flags,
									 GTK_MESSAGE_ERROR,
									 GTK_BUTTONS_OK,
									 error->message,
									 NULL);
			gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);
			g_error_free(error);
			return;
		}

		vte_terminal_search_set_regex(term, regex, 0);
	}

	if (direction == FIND_PREVIOUS)
		vte_terminal_search_find_previous(term);
	else
		vte_terminal_search_find_next(term);
}


static gboolean entry_key_press_event_callback(GtkEntry *entry, GdkEventKey *event, GtkWidget *searchBar)
{
	guint mask = gtk_accelerator_get_default_mod_mask();
	gboolean handled = FALSE;

	/*
	 * Additional search keybindings
	 * Escape key: Close search toolbar
	 * Shift + Enter: Go to previous search result
	 */
	if ((event->state & mask) == 0) {
		handled = TRUE;
		switch (event->keyval) {
			case GDK_KEY_Escape:
				search_bar_hide(searchBar);
				break;
			default:
				handled = FALSE;
				break;
		}
	} else if ((event->state & mask) == GDK_SHIFT_MASK &&
						 (event->keyval == GDK_KEY_Return ||
							event->keyval == GDK_KEY_KP_Enter ||
							event->keyval == GDK_KEY_ISO_Enter)) {
		handled = TRUE;
		search_callback(NULL, GUINT_TO_POINTER(FIND_PREVIOUS));
	}

	return handled;
}

GtkWidget *search_bar_new(GtkWindow *parent, VteTerminal *terminal)
{
	parentWindow = parent;
	term = terminal;
	regex = NULL;
	vte_terminal_search_set_wrap_around(term, TRUE);

	searchBar = gtk_search_bar_new();
	gtk_search_bar_connect_entry(GTK_SEARCH_BAR(searchBar), GTK_ENTRY(entry));
	gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(searchBar), FALSE);
	gtk_search_bar_set_show_close_button (GTK_SEARCH_BAR(searchBar), TRUE);

	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_style_context_add_class(gtk_widget_get_style_context(box), "linked");
	gtk_container_add(GTK_CONTAINER(searchBar), box);

	entry = gtk_search_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 30);
	gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
	g_signal_connect(entry, "changed", G_CALLBACK(entry_changed_callback), NULL);
	g_signal_connect(entry, "key-press-event", G_CALLBACK(entry_key_press_event_callback), searchBar);

	prevImage = gtk_image_new_from_icon_name("go-up-symbolic", GTK_ICON_SIZE_MENU);
	prevButton = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(prevButton), prevImage);
	gtk_box_pack_start(GTK_BOX(box), prevButton, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(prevButton), "clicked", G_CALLBACK(search_callback), GUINT_TO_POINTER(FIND_PREVIOUS));
	gtk_widget_set_sensitive(prevButton, FALSE);

	nextImage = gtk_image_new_from_icon_name("go-down-symbolic", GTK_ICON_SIZE_MENU);
	nextButton = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(nextButton), nextImage);
	gtk_box_pack_start(GTK_BOX(box), nextButton, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(nextButton), "clicked", G_CALLBACK(search_callback), GUINT_TO_POINTER(FIND_NEXT));
	gtk_widget_set_sensitive(nextButton, FALSE);

	return searchBar;
}

void search_bar_show(GtkWidget *self)
{
	gtk_widget_show(self);
	gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(searchBar), TRUE);

	gtk_widget_grab_focus (entry);

	/* Set Enter key to "press" next button by default */
	gtk_widget_set_can_default(nextButton, TRUE);
	gtk_widget_grab_default(nextButton);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
}

void search_bar_hide(GtkWidget *self)
{
	gtk_widget_hide(self);
	vte_terminal_search_set_regex(term, NULL, 0);
	gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(searchBar), FALSE);
}
