#include <gtk/gtk.h>
#include <ctype.h>
#include <glib/gi18n.h>

#include "term_config.h"
#include "config_file.h"
#include "interface.h"
#include "serial.h"
#include "macros.h"
#include "config_dialog.h"

static void on_load_ok_clicked(GtkListBox *, GtkButton *);
static void on_delete_ok_clicked(GtkListBox *, GtkButton *);
static void on_save_ok_clicked(GtkEditable *, GtkButton *);

static void Select_config(gchar *title, GCallback on_ok)
{
	gchar **groups;
	gchar **g;
	GtkBuilderCScope *scope;
	GtkBuilder *builder;
	GtkWindow *dialog;
	GtkListBox *listbox;

	scope = GTK_BUILDER_CSCOPE(gtk_builder_cscope_new());
	gtk_builder_cscope_add_callback_symbols(scope,
	    "on_ok_clicked",                on_ok,
	    "gtk_window_destroy",           G_CALLBACK(gtk_window_destroy),
	    "gtk_widget_activate_default",  G_CALLBACK(gtk_widget_activate_default),
	    NULL);

	builder = gtk_builder_new();
	gtk_builder_set_scope(builder, GTK_BUILDER_SCOPE(scope));
	g_object_unref(scope);
	gtk_builder_add_from_resource(builder, "/org/gtk/gtkterm/select_config_dialog.ui", NULL);

	dialog  = GTK_WINDOW(gtk_builder_get_object(builder, "select_config_window"));
	listbox = GTK_LIST_BOX(gtk_builder_get_object(builder, "select_config_list"));
	g_object_unref(builder);

	gtk_window_set_title(dialog, title);
	gtk_window_set_transient_for(dialog, GTK_WINDOW(Fenetre));

	groups = config_get_sections();
	for (g = groups; *g; g++)
		gtk_list_box_append(listbox, gtk_label_new(*g));
	g_strfreev(groups);

	gtk_window_present(dialog);
}

static void check_text_input_alnum(GtkEditable *editable, gchar *new_text,
                                   gint new_text_length, gint *position,
                                   gpointer user_data G_GNUC_UNUSED)
{
	check_text_input(editable, new_text, new_text_length, position, isalnum);
}

static void Save_config_file(void)
{
	GtkBuilderCScope *scope;
	GtkBuilder *builder;
	GtkWindow *dialog;

	scope = GTK_BUILDER_CSCOPE(gtk_builder_cscope_new());
	gtk_builder_cscope_add_callback_symbols(scope,
		"check_text_input_alnum", G_CALLBACK(check_text_input_alnum),
		"on_save_ok_clicked",     G_CALLBACK(on_save_ok_clicked),
		"gtk_window_destroy",     G_CALLBACK(gtk_window_destroy),
		NULL);

	builder = gtk_builder_new();
	gtk_builder_set_scope(builder, GTK_BUILDER_SCOPE(scope));
	g_object_unref(scope);
	gtk_builder_add_from_resource(builder, "/org/gtk/gtkterm/save_config_dialog.ui", NULL);

	dialog = GTK_WINDOW(gtk_builder_get_object(builder, "save_config_window"));
	gtk_window_set_transient_for(dialog, GTK_WINDOW(Fenetre));
	gtk_window_present(dialog);
}

static void on_overwrite_response(GObject *source, GAsyncResult *result, gpointer data)
{
	int response = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(source), result, NULL);
	if (response == 0)
		Save_configuration_to_file(data);
	g_free(data);
}

static void on_save_ok_clicked(GtkEditable *entry, GtkButton *btn G_GNUC_UNUSED)
{
	const gchar *config_name = gtk_editable_get_text(GTK_EDITABLE(entry));

	if (config_section_exists(config_name) && !g_str_equal(config_name, "default"))
	{
		static const char * const buttons[] = { "_Yes", "_Cancel", NULL };
		g_autofree gchar *msg = g_strdup_printf(
		    _("Section [%s] already exists.\n\nDo you want to overwrite it ?"),
		    config_name);

		GtkAlertDialog *alert = gtk_alert_dialog_new("%s", msg);
		gtk_alert_dialog_set_modal(alert, TRUE);
		gtk_alert_dialog_set_buttons(alert, buttons);
		gtk_alert_dialog_set_default_button(alert, 0);
		gtk_alert_dialog_set_cancel_button(alert, 1);
		gtk_alert_dialog_choose(alert, GTK_WINDOW(Fenetre), NULL,
		                        on_overwrite_response, g_strdup(config_name));
		g_object_unref(alert);
	}
	else
		Save_configuration_to_file(config_name);
}

static void on_load_ok_clicked(GtkListBox *listbox, GtkButton *btn G_GNUC_UNUSED)
{
	GtkListBoxRow *row = gtk_list_box_get_selected_row(listbox);
	if (row)
	{
		const gchar *txt = gtk_label_get_text(GTK_LABEL(gtk_list_box_row_get_child(row)));
		if (Load_configuration_from_file(txt) == -1)
		{
			show_messagef(MSG_ERR, _("No section \"%s\" in configuration file\n"), txt);
			return;
		}

		interface_apply_term_config();
		Config_port();
		ConfigFlags();
		update_port_status();
	}
}

static void on_delete_ok_clicked(GtkListBox *listbox, GtkButton *btn G_GNUC_UNUSED)
{
	GtkListBoxRow *row = gtk_list_box_get_selected_row(listbox);
	if (row)
	{
		const gchar *txt = gtk_label_get_text(GTK_LABEL(gtk_list_box_row_get_child(row)));
		if (!config_delete_section(txt))
			show_message(MSG_ERR, _("Cannot delete section!"));
	}
}

void select_config_callback(GSimpleAction *action G_GNUC_UNUSED, GVariant *param G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	Select_config(_("Load configuration"), G_CALLBACK(on_load_ok_clicked));
}

void save_config_callback(GSimpleAction *action G_GNUC_UNUSED, GVariant *param G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	if (Fenetre)
	{
		int w = gtk_widget_get_width(GTK_WIDGET(Fenetre));
		int h = gtk_widget_get_height(GTK_WIDGET(Fenetre));
		if (w > 0 && h > 0)
		{
			term_conf.window_width  = w;
			term_conf.window_height = h;
		}
	}
	Save_config_file();
}

void delete_config_callback(GSimpleAction *action G_GNUC_UNUSED, GVariant *param G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	Select_config(_("Delete configuration"), G_CALLBACK(on_delete_ok_clicked));
}
