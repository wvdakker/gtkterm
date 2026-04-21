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

#include <glib/gi18n.h>


static macro_t *macros = NULL;
static gsize macros_count = 0;
static GtkWindow *window = NULL;
static GtkListBox *listbox = NULL;

/* Capture-phase key controller on the main window */
static GtkEventController *macro_key_ctrl = NULL;
static gboolean macros_enabled = TRUE;

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
	g_object_ref(row); /* keep alive after builder releases it */
	g_object_unref(b);
	return row; /* caller must g_object_unref after parenting */
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

static gboolean macro_key_pressed(GtkEventControllerKey *controller G_GNUC_UNUSED,
                                   guint keyval, guint keycode G_GNUC_UNUSED,
                                   GdkModifierType state, gpointer data G_GNUC_UNUSED)
{
	guint i;

	if (!macros_enabled || macros == NULL)
		return FALSE;

	state &= gtk_accelerator_get_default_mod_mask();

	for (i = 0; i < macros_count; i++)
	{
		if (macros[i].keyval == keyval && macros[i].mods == state)
		{
			g_autofree gchar *msg;

			send_serial(macros[i].expanded, macros[i].expanded_len);
			msg = g_strdup_printf(_("Macro \"%s\" sent!"), macros[i].shortcut);
			Put_temp_message(msg, 800);
			return TRUE;
		}
	}
	return FALSE;
}

void install_macro_shortcut_controller(GtkWidget *win)
{
	if (macro_key_ctrl != NULL)
		return;

	macro_key_ctrl = gtk_event_controller_key_new();
	gtk_event_controller_set_propagation_phase(macro_key_ctrl, GTK_PHASE_CAPTURE);
	g_signal_connect(macro_key_ctrl, "key-pressed",
	                 G_CALLBACK(macro_key_pressed), NULL);
	gtk_widget_add_controller(win, macro_key_ctrl);
}

void set_macros_shortcuts_enabled(gboolean enabled)
{
	macros_enabled = enabled;
}

void create_shortcuts(macro_t *macro, gsize size)
{
	guint i;

	remove_shortcuts();

	if (size == 0)
	{
		g_free(macro);
		return;
	}

	for (i = 0; i < (guint)size; i++)
	{
		macro[i].keyval   = 0;
		macro[i].mods     = 0;
		macro[i].expanded     = g_strcompress(macro[i].action);
		macro[i].expanded_len = strlen(macro[i].expanded);
		gtk_accelerator_parse(macro[i].shortcut, &macro[i].keyval, &macro[i].mods);
	}

	macros       = macro;
	macros_count = (guint)size;
}

void remove_shortcuts(void)
{
	guint i;

	if (macros == NULL)
		return;

	for (i = 0; i < macros_count; i++)
	{
		g_free(macros[i].shortcut);
		g_free(macros[i].action);
		g_free(macros[i].expanded);
	}
	g_free(macros);
	macros = NULL;
	macros_count = 0;
}

/* ---- GtkListBox-based macro editor ---- */

static void Add_shortcut(GtkWidget *button G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	GListModel *children;
	GtkListBoxRow *new_row;
	GtkWidget *new_widget;
	guint n;

	new_widget = create_macro_row("None", "");
	gtk_list_box_append(listbox, new_widget);
	g_object_unref(new_widget);

	children = gtk_widget_observe_children(GTK_WIDGET(listbox));
	n = g_list_model_get_n_items(children);
	new_row = gtk_list_box_get_row_at_index(listbox, n - 1);
	if (new_row)
		gtk_list_box_select_row(listbox, new_row);
	g_object_unref(children);
}

static void Delete_shortcut(GtkWidget *button G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	GtkListBoxRow *row = gtk_list_box_get_selected_row(listbox);

	if (row)
		gtk_list_box_remove(listbox, GTK_WIDGET(row));
}

static void Save_shortcuts(GtkWidget *button G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
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
                                    guint keyval, guint keycode G_GNUC_UNUSED,
                                    GdkModifierType state, gpointer listbox_ptr G_GNUC_UNUSED)
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

	row = gtk_list_box_get_selected_row(listbox);
	if (row)
	{
		str = gtk_accelerator_name(keyval,
		                           state & gtk_accelerator_get_default_mod_mask());
		gtk_label_set_text(GTK_LABEL(row_label(row)), str);
		g_free(str);
	}

	g_signal_handlers_disconnect_by_func(controller,
	                                     G_CALLBACK(key_capture_pressed), NULL);
	return TRUE;
}

static void Capture_shortcut(GtkWidget *button G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	GtkEventController *ctrl = gtk_event_controller_key_new();

	g_signal_connect(ctrl, "key-pressed",
	                 G_CALLBACK(key_capture_pressed), NULL);
	gtk_widget_add_controller(GTK_WIDGET(window), ctrl);
}

static void Help_screen(GtkWidget *button G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	GtkAlertDialog *Dialog = gtk_alert_dialog_new("%s",
	    _("The \"action\" field of a macro is the data to be sent on the port. "
	      "Text can be entered, but also special chars, like \\n, \\t, \\r, etc. "
	      "You can also enter hexadecimal data preceded by a '\\x'. "
	      "Examples:\n"
	      "\t\"Hello\\n\" sends \"Hello\" followed by a Line Feed\n"
	      "\t\"Hello\\x0A\" does the same thing but the LF is entered in hexadecimal"));
	gtk_alert_dialog_set_modal(Dialog, TRUE);
	gtk_alert_dialog_show(Dialog, window);
	g_object_unref(Dialog);
}

static void on_ok_clicked(GtkWidget *button, gpointer data G_GNUC_UNUSED)
{
	Save_shortcuts(button, NULL);
	gtk_window_close(window);
}

static void on_cancel_clicked(GtkWidget *button G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	gtk_window_close(window);
}

static gboolean on_macros_close_request(GtkWindow *win G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	window = NULL;
	listbox = NULL;
	return FALSE; /* allow default destruction */
}

void Config_macros(GSimpleAction *action G_GNUC_UNUSED, GVariant *param G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	GtkBuilderCScope *scope;
	GtkBuilder *builder;

	if (window != NULL)
	{
		gtk_window_present(window);
		return;
	}

	scope = GTK_BUILDER_CSCOPE(gtk_builder_cscope_new());
	gtk_builder_cscope_add_callback_symbols(scope,
	    "on_macros_close_request", G_CALLBACK(on_macros_close_request),
	    "Add_shortcut",            G_CALLBACK(Add_shortcut),
	    "Delete_shortcut",         G_CALLBACK(Delete_shortcut),
	    "Capture_shortcut",        G_CALLBACK(Capture_shortcut),
	    "Help_screen",             G_CALLBACK(Help_screen),
	    "on_ok_clicked",           G_CALLBACK(on_ok_clicked),
	    "on_cancel_clicked",       G_CALLBACK(on_cancel_clicked),
	    NULL);

	builder = gtk_builder_new();
	gtk_builder_set_scope(builder, GTK_BUILDER_SCOPE(scope));
	g_object_unref(scope);
	gtk_builder_add_from_resource(builder, "/org/gtk/gtkterm/macros_dialog.ui", NULL);

	window  = GTK_WINDOW(gtk_builder_get_object(builder, "macros_window"));
	listbox = GTK_LIST_BOX(gtk_builder_get_object(builder, "macros_listbox"));
	g_object_unref(builder);

	gtk_window_set_transient_for(window, Fenetre);

	/* Populate from existing macros */
	if (macros != NULL)
	{
		GtkWidget *row_box;
		guint i;

		for (i = 0; i < macros_count; i++)
		{
			row_box = create_macro_row(macros[i].shortcut, macros[i].action);
			gtk_list_box_append(listbox, row_box);
			g_object_unref(row_box);
		}
	}

	gtk_window_present(window);
}
