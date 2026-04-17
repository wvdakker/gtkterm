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
/*      Ported to GTK4                                                 */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#include <string.h>

#include "interface.h"
#include "macros.h"

#include <config.h>
#include <glib/gi18n.h>


static macro_t *macros = NULL;
static gsize macros_count = 0;
static GtkWidget *window = NULL;

/* Shortcut controller attached to the main window */
static GtkShortcutController *macro_ctrl = NULL;
static GPtrArray *macro_shortcuts = NULL; /* owned GtkShortcut* refs */

/* ---- Helper: build one row from the UI template ---- */

/* Create a single row widget for a macro entry (shortcut label + action entry) */
static GtkWidget *create_macro_row(const gchar *shortcut, const gchar *action)
{
	GtkBuilder *b = gtk_builder_new_from_resource("/org/gtk/gtkterm/macro_row.ui");
	GtkWidget *row   = GTK_WIDGET(gtk_builder_get_object(b, "macro_row"));
	GtkWidget *label = GTK_WIDGET(gtk_builder_get_object(b, "macro_shortcut_label"));
	GtkWidget *entry = GTK_WIDGET(gtk_builder_get_object(b, "macro_action_entry"));

	gtk_label_set_text(GTK_LABEL(label), shortcut ? shortcut : "None");
	gtk_editable_set_text(GTK_EDITABLE(entry), action ? action : "");
	g_object_unref(b);
	return row;
}

/* ---- Row accessors ---- */

static GtkWidget *row_label(GtkListBoxRow *row)
{
	GtkWidget *box = gtk_list_box_row_get_child(row);
	return gtk_widget_get_first_child(box);
}

static GtkWidget *row_entry(GtkListBoxRow *row)
{
	GtkWidget *box = gtk_list_box_row_get_child(row);
	return gtk_widget_get_last_child(box);
}

macro_t *get_shortcuts(gsize *size)
{
	*size = macros_count;
	return macros;
}

static void shortcut_callback(gpointer *number); /* forward declaration */

static gboolean macro_shortcut_activated(GtkWidget *widget, GVariant *args, gpointer user_data)
{
	shortcut_callback(user_data);
	return TRUE;
}

static void shortcut_callback(gpointer *number)
{
	g_autofree gchar *expanded;
	g_autofree gchar *msg;

	expanded = g_strcompress(macros[(gintptr)number].action);
	send_serial(expanded, (gint)strlen(expanded));

	msg = g_strdup_printf(_("Macro \"%s\" sent!"), macros[(gintptr)number].shortcut);
	Put_temp_message(msg, 800);
}

void install_macro_shortcut_controller(GtkShortcutController *ctrl)
{
	if (macro_ctrl != NULL)
		return;

	macro_ctrl = ctrl;
	macro_shortcuts = g_ptr_array_new_with_free_func(g_object_unref);
}

void set_macros_shortcuts_enabled(gboolean enabled)
{
	if (macro_ctrl == NULL)
		return;
	gtk_event_controller_set_propagation_phase(
		GTK_EVENT_CONTROLLER(macro_ctrl),
		enabled ? GTK_PHASE_BUBBLE : GTK_PHASE_NONE);
}

void create_shortcuts(macro_t *macro, gsize size)
{
	guint i;
	guint keyval;
	GdkModifierType mods;
	GtkShortcutTrigger *trigger;
	GtkShortcutAction  *act;
	GtkShortcut *sc;

	remove_shortcuts();

	if (size == 0)
	{
		g_free(macro);
		return;
	}

	macros       = macro;
	macros_count = (guint)size;

	/* Register GtkShortcut entries for each macro */
	if (macro_ctrl != NULL)
	{
		for (i = 0; i < macros_count; i++)
		{
			keyval = 0;
			mods = 0;
			if (!gtk_accelerator_parse(macros[i].shortcut, &keyval, &mods) || keyval == 0)
				continue;

			trigger = gtk_keyval_trigger_new(keyval, mods);
			act     = gtk_callback_action_new(
				macro_shortcut_activated, GINT_TO_POINTER(i), NULL);

			sc = gtk_shortcut_new(trigger, act);
			gtk_shortcut_controller_add_shortcut(macro_ctrl, g_object_ref(sc));
			g_ptr_array_add(macro_shortcuts, sc); /* takes ownership */
		}
	}
}

void remove_shortcuts(void)
{
	guint i;

	/* Remove GtkShortcut registrations */
	if (macro_ctrl != NULL && macro_shortcuts != NULL)
	{
		for (i = 0; i < macro_shortcuts->len; i++)
			gtk_shortcut_controller_remove_shortcut(
				macro_ctrl, g_ptr_array_index(macro_shortcuts, i));
		g_ptr_array_set_size(macro_shortcuts, 0);
	}

	if (macros == NULL)
		return;

	for (i = 0; i < macros_count; i++)
	{
		g_free(macros[i].shortcut);
		g_free(macros[i].action);
	}
	g_free(macros);
	macros = NULL;
	macros_count = 0;
}

void macros_cleanup(void)
{
	guint i;

	/* Free macro string data. Do NOT touch macro_ctrl here — the
	 * GtkShortcutController belongs to the main window which is already
	 * destroyed by the time on_shutdown() runs. */
	if (macros != NULL)
	{
		for (i = 0; i < macros_count; i++)
		{
			g_free(macros[i].shortcut);
			g_free(macros[i].action);
		}
		g_free(macros);
		macros = NULL;
		macros_count = 0;
	}

	/* Free the GPtrArray — its free_func (g_object_unref) will release any
	 * GtkShortcut objects that outlived the controller. */
	if (macro_shortcuts != NULL)
	{
		g_ptr_array_free(macro_shortcuts, TRUE);
		macro_shortcuts = NULL;
	}
	macro_ctrl = NULL;
}

/* ---- GtkListBox-based macro editor ---- */

static void Add_shortcut(GtkWidget *button, gpointer listbox_ptr)
{
	GtkListBox *listbox = GTK_LIST_BOX(listbox_ptr);
	GListModel *children;
	GtkListBoxRow *new_row;
	guint n;

	gtk_list_box_append(listbox, create_macro_row("None", ""));
	children = gtk_widget_observe_children(GTK_WIDGET(listbox));
	n = g_list_model_get_n_items(children);
	new_row = gtk_list_box_get_row_at_index(listbox, n - 1);
	if (new_row)
		gtk_list_box_select_row(listbox, new_row);
	g_object_unref(children);
}

static void Delete_shortcut(GtkWidget *button, gpointer listbox_ptr)
{
	GtkListBox *listbox = GTK_LIST_BOX(listbox_ptr);
	GtkListBoxRow *row = gtk_list_box_get_selected_row(listbox);
	if (row)
		gtk_list_box_remove(listbox, GTK_WIDGET(row));
}

static void Save_shortcuts(GtkWidget *button, gpointer listbox_ptr)
{
	GtkListBox *listbox = GTK_LIST_BOX(listbox_ptr);
	GListModel *children;
	macro_t *new_macros;
	guint n;

	children = gtk_widget_observe_children(GTK_WIDGET(listbox));
	n = g_list_model_get_n_items(children);

	remove_shortcuts();
	if (n == 0)
		return;

	new_macros = g_malloc(n * sizeof(macro_t));
	if (new_macros != NULL)
	{
		GtkListBoxRow *row;
		const gchar *sc;
		guint i;
		guint m = 0;

		for (i = 0; i < n; i++)
		{
			row = GTK_LIST_BOX_ROW(g_list_model_get_item(children, i));
			sc  = gtk_label_get_text(GTK_LABEL(row_label(row)));
			if (sc && *sc && strcmp(sc, "None") != 0)
			{
				new_macros[m].shortcut = g_strdup(sc);
				new_macros[m].action   = g_strdup(gtk_editable_get_text(GTK_EDITABLE(row_entry(row))));
				m++;
			}
			g_object_unref(row);
		}
		create_shortcuts(new_macros, m); /* transfers ownership */
	}
}

/* Capture shortcut: key-press event recorded via GtkEventControllerKey */

static gboolean key_capture_pressed(GtkEventControllerKey *controller,
                                    guint keyval, guint keycode,
                                    GdkModifierType state, gpointer listbox_ptr)
{
	GtkListBoxRow *row;
	gchar *str;

	/* Ignore pure modifier keys */
	switch (keyval)
	{
	case GDK_KEY_Shift_L:   case GDK_KEY_Shift_R:
	case GDK_KEY_Control_L: case GDK_KEY_Control_R:
	case GDK_KEY_Caps_Lock: case GDK_KEY_Shift_Lock:
	case GDK_KEY_Meta_L:    case GDK_KEY_Meta_R:
	case GDK_KEY_Alt_L:     case GDK_KEY_Alt_R:
	case GDK_KEY_Super_L:   case GDK_KEY_Super_R:
	case GDK_KEY_Hyper_L:   case GDK_KEY_Hyper_R:
	case GDK_KEY_Mode_switch:
		return FALSE;
	default:
		break;
	}

	row = gtk_list_box_get_selected_row(GTK_LIST_BOX(listbox_ptr));
	if (row)
	{
		str = gtk_accelerator_name(keyval,
		                           state & gtk_accelerator_get_default_mod_mask());
		gtk_label_set_text(GTK_LABEL(row_label(row)), str);
		g_free(str);
	}

	g_signal_handlers_disconnect_by_func(controller,
	                                     G_CALLBACK(key_capture_pressed), listbox_ptr);
	return TRUE;
}

static void Capture_shortcut(GtkWidget *button, gpointer listbox_ptr)
{
	GtkEventController *ctrl = gtk_event_controller_key_new();

	g_signal_connect(ctrl, "key-pressed",
	                 G_CALLBACK(key_capture_pressed), listbox_ptr);
	gtk_widget_add_controller(window, ctrl);
}

static void Help_screen(GtkWidget *button, gpointer pointer)
{
	GtkAlertDialog *Dialog = gtk_alert_dialog_new("%s",
	    _("The \"action\" field of a macro is the data to be sent on the port. "
	      "Text can be entered, but also special chars, like \\n, \\t, \\r, etc. "
	      "You can also enter hexadecimal data preceded by a '\\x'. "
	      "Examples:\n"
	      "\t\"Hello\\n\" sends \"Hello\" followed by a Line Feed\n"
	      "\t\"Hello\\x0A\" does the same thing but the LF is entered in hexadecimal"));
	gtk_alert_dialog_set_modal(Dialog, TRUE);
	gtk_alert_dialog_show(Dialog, GTK_WINDOW(pointer));
	g_object_unref(Dialog);
}

static gboolean on_macros_close_request(GtkWindow *win, gpointer data)
{
	window = NULL;
	return FALSE; /* allow default destruction */
}

void Config_macros(GSimpleAction *action, GVariant *param, gpointer data)
{
	GtkWidget *listbox;
	GtkBuilderCScope *scope;
	GtkBuilder *builder;

	if (window != NULL)
	{
		gtk_window_present(GTK_WINDOW(window));
		return;
	}

	scope = GTK_BUILDER_CSCOPE(gtk_builder_cscope_new());
	gtk_builder_cscope_add_callback_symbols(scope,
	    "on_macros_close_request", G_CALLBACK(on_macros_close_request),
	    "Add_shortcut",            G_CALLBACK(Add_shortcut),
	    "Delete_shortcut",         G_CALLBACK(Delete_shortcut),
	    "Capture_shortcut",        G_CALLBACK(Capture_shortcut),
	    "Help_screen",             G_CALLBACK(Help_screen),
	    "Save_shortcuts",          G_CALLBACK(Save_shortcuts),
	    "gtk_window_close",        G_CALLBACK(gtk_window_close),
	    NULL);

	builder = gtk_builder_new();
	gtk_builder_set_scope(builder, GTK_BUILDER_SCOPE(scope));
	g_object_unref(scope);
	gtk_builder_add_from_resource(builder, "/org/gtk/gtkterm/macros_dialog.ui", NULL);

	window  = GTK_WIDGET(gtk_builder_get_object(builder, "macros_window"));
	listbox = GTK_WIDGET(gtk_builder_get_object(builder, "macros_listbox"));
	g_object_unref(builder);

	gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(Fenetre));

	/* Populate from existing macros */
	if (macros != NULL)
	{
		GtkWidget *row_box;
		guint i;

		for (i = 0; i < macros_count; i++)
		{
			row_box = create_macro_row(macros[i].shortcut, macros[i].action);
			gtk_list_box_append(GTK_LIST_BOX(listbox), row_box);
		}
	}

	gtk_window_present(GTK_WINDOW(window));
}
