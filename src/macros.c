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



macro_t *macros = NULL;
static GtkWidget *window = NULL;

macro_t *get_shortcuts(gint *size)
{
	gint i = 0;

	if (macros != NULL)
	{
		while (macros[i].shortcut != NULL)
			i++;
	}
	*size = i;
	return macros;
}

static void shortcut_callback(gpointer *number)
{
	gchar *string;
	gchar *str;
	gint i, length;
	guchar a;
	gchar *end;

	string = macros[(gintptr)number].action;
	length = (gint)strlen(string);

	for (i = 0; i < length; i++)
	{
		if (string[i] == '\\')
		{
			if (g_unichar_isdigit((gunichar)string[i + 1]))
			{
				if ((string[i + 1] == '0') && (string[i + 2] != 0))
				{
					if (g_unichar_isxdigit((gunichar)string[i + 3]))
					{
						str = &string[i + 2];
						i += 3;
					}
					else
					{
						str = &string[i + 1];
						if (g_unichar_isxdigit((gunichar)string[i + 2]))
							i += 2;
						else
							i++;
					}
				}
				else
				{
					str = &string[i + 1];
					if (g_unichar_isxdigit((gunichar)string[i + 2]))
						i += 2;
					else
						i++;
				}
				a = (guchar)g_ascii_strtoull(str, &end, 16);
				if (end == str)
					a = '\\';
			}
			else
			{
				switch (string[i + 1])
				{
				case 'a': a = '\a'; break;
				case 'b': a = '\b'; break;
				case 't': a = '\t'; break;
				case 'n': a = '\n'; break;
				case 'v': a = '\v'; break;
				case 'f': a = '\f'; break;
				case 'r': a = '\r'; break;
				case '\\': a = '\\'; break;
				default:
					a = '\\';
					i--;
					break;
				}
				i++;
			}
			send_serial((gchar *)&a, 1);
		}
		else
		{
			send_serial(&string[i], 1);
		}
	}

	g_autofree gchar *msg = g_strdup_printf(_("Macro \"%s\" sent!"), macros[(gintptr)number].shortcut);
	Put_temp_message(msg, 800);
}

/* Process a key event and trigger a macro if a match is found */
gboolean macros_process_key(guint keyval, GdkModifierType state)
{
	gintptr i;
	guint acc_key;
	GdkModifierType mod;

	if (macros == NULL)
		return FALSE;

	for (i = 0; macros[i].shortcut != NULL; i++)
	{
		acc_key = 0;
		mod = 0;
		gtk_accelerator_parse(macros[i].shortcut, &acc_key, &mod);
		if (acc_key != 0 && keyval == acc_key &&
		    (state & gtk_accelerator_get_default_mod_mask()) == mod)
		{
			shortcut_callback((gpointer)i);
			return TRUE;
		}
	}
	return FALSE;
}

void create_shortcuts(macro_t *macro, gint size)
{
	gint i;

	remove_shortcuts();

	macros = g_malloc((size + 1) * sizeof(macro_t));
	for (i = 0; i < size; i++)
	{
		macros[i].shortcut = g_strdup(macro[i].shortcut);
		macros[i].action   = g_strdup(macro[i].action);
	}
	macros[size].shortcut = NULL;
	macros[size].action = NULL;
}

void remove_shortcuts(void)
{
	gint i = 0;

	if (macros == NULL)
		return;

	while (macros[i].shortcut != NULL)
	{
		g_free(macros[i].shortcut);
		g_free(macros[i].action);
		i++;
	}
	g_free(macros);
	macros = NULL;
}

/* ---- GtkListBox-based macro editor ---- */

static gint listbox_count_rows(GtkListBox *listbox)
{
	gint n = 0;
	while (gtk_list_box_get_row_at_index(listbox, n) != NULL)
		n++;
	return n;
}

/* Create a single row widget for a macro entry (shortcut label + action entry) */
static GtkWidget *create_macro_row(const gchar *shortcut, const gchar *action)
{
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_set_margin_start(row, 4);
	gtk_widget_set_margin_end(row, 4);
	gtk_widget_set_margin_top(row, 2);
	gtk_widget_set_margin_bottom(row, 2);

	GtkWidget *sc_label = gtk_label_new(shortcut ? shortcut : "None");
	gtk_label_set_xalign(GTK_LABEL(sc_label), 0.0f);
	gtk_widget_set_size_request(sc_label, 150, -1);

	GtkWidget *act_entry = gtk_entry_new();
	gtk_widget_set_hexpand(act_entry, TRUE);
	if (action)
		gtk_editable_set_text(GTK_EDITABLE(act_entry), action);

	gtk_box_append(GTK_BOX(row), sc_label);
	gtk_box_append(GTK_BOX(row), act_entry);
	return row;
}

static void Add_shortcut(GtkWidget *button, gpointer listbox_ptr)
{
	GtkListBox *listbox = GTK_LIST_BOX(listbox_ptr);
	gint n = listbox_count_rows(listbox);
	GtkWidget *row_box = create_macro_row("None", "");
	gtk_list_box_append(listbox, row_box);
	GtkListBoxRow *new_row = gtk_list_box_get_row_at_index(listbox, n);
	if (new_row)
		gtk_list_box_select_row(listbox, new_row);
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
	gint n = listbox_count_rows(listbox);

	remove_shortcuts();
	if (n == 0)
		return;

	macros = g_malloc((n + 1) * sizeof(macro_t));
	if (macros != NULL)
	{
		for (gint i = 0; i < n; i++)
		{
			GtkListBoxRow *row = gtk_list_box_get_row_at_index(listbox, i);
			GtkWidget *box = gtk_list_box_row_get_child(row);
			GtkWidget *sc_label = gtk_widget_get_first_child(box);
			GtkWidget *act_entry = gtk_widget_get_last_child(box);
			macros[i].shortcut = g_strdup(gtk_label_get_text(GTK_LABEL(sc_label)));
			macros[i].action = g_strdup(gtk_editable_get_text(GTK_EDITABLE(act_entry)));
		}
		macros[n].shortcut = NULL;
		macros[n].action = NULL;
	}
}

/* Capture shortcut: key-press event recorded via GtkEventControllerKey */

static gboolean key_capture_pressed(GtkEventControllerKey *controller,
                                    guint keyval, guint keycode,
                                    GdkModifierType state, gpointer pointer)
{
	GtkListBox *listbox = GTK_LIST_BOX(pointer);

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

	GtkListBoxRow *row = gtk_list_box_get_selected_row(listbox);
	if (row)
	{
		GtkWidget *box = gtk_list_box_row_get_child(row);
		GtkWidget *sc_label = gtk_widget_get_first_child(box);
		gchar *str = gtk_accelerator_name(keyval,
		                                  state & gtk_accelerator_get_default_mod_mask());
		gtk_label_set_text(GTK_LABEL(sc_label), str);
		g_free(str);
	}

	/* Block further captures — remove on next idle when safe */
	g_signal_handlers_disconnect_by_func(controller,
	                                     G_CALLBACK(key_capture_pressed), pointer);
	return TRUE;
}

static void Capture_shortcut(GtkWidget *button, gpointer pointer)
{
	GtkEventController *ctrl = gtk_event_controller_key_new();
	g_signal_connect(ctrl, "key-pressed",
	                 G_CALLBACK(key_capture_pressed), pointer);
	gtk_widget_add_controller(window, ctrl);
}

static void Help_screen(GtkWidget *button, gpointer pointer)
{
	GtkAlertDialog *Dialog = gtk_alert_dialog_new("%s",
	    _("The \"action\" field of a macro is the data to be sent on the port. "
	      "Text can be entered, but also special chars, like \\n, \\t, \\r, etc. "
	      "You can also enter hexadecimal data preceded by a '\\'. "
	      "The hexadecimal data should not begin with a letter "
	      "(eg. use \\0FF and not \\FF)\n"
	      "Examples:\n"
	      "\t\"Hello\\n\" sends \"Hello\" followed by a Line Feed\n"
	      "\t\"Hello\\0A\" does the same thing but the LF is entered in hexadecimal"));
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

	if (window != NULL)
	{
		gtk_window_present(GTK_WINDOW(window));
		return;
	}

	GtkBuilderCScope *scope = GTK_BUILDER_CSCOPE(gtk_builder_cscope_new());
	gtk_builder_cscope_add_callback_symbols(scope,
	    "on_macros_close_request", G_CALLBACK(on_macros_close_request),
	    "Add_shortcut",            G_CALLBACK(Add_shortcut),
	    "Delete_shortcut",         G_CALLBACK(Delete_shortcut),
	    "Capture_shortcut",        G_CALLBACK(Capture_shortcut),
	    "Help_screen",             G_CALLBACK(Help_screen),
	    "Save_shortcuts",          G_CALLBACK(Save_shortcuts),
	    "gtk_window_close",        G_CALLBACK(gtk_window_close),
	    NULL);

	GtkBuilder *builder = gtk_builder_new();
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
		gint i = 0;
		while (macros[i].shortcut != NULL)
		{
			GtkWidget *row_box = create_macro_row(macros[i].shortcut, macros[i].action);
			gtk_list_box_append(GTK_LIST_BOX(listbox), row_box);
			i++;
		}
	}

	gtk_window_present(GTK_WINDOW(window));
}
