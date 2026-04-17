#include <gtk/gtk.h>
#include <ctype.h>
#include <glib/gi18n.h>

#include "term_config.h"
#include "config_file.h"
#include "interface.h"
#include "serial.h"
#include "macros.h"
#include "config_dialog.h"

static void Save_config_file(void);
static void load_config(gpointer, gint, GtkSingleSelection *);
static void delete_config(gpointer, gint, GtkSingleSelection *);
static void save_config(gpointer, gint, GtkWidget *);
static void really_save_config(gpointer, gint, gconstpointer);

static void on_load_ok_clicked(GtkSingleSelection *sel, GtkButton *btn)
{
	(void)btn;
	load_config(NULL, GTK_RESPONSE_ACCEPT, sel);
}

static void on_delete_ok_clicked(GtkSingleSelection *sel, GtkButton *btn)
{
	(void)btn;
	delete_config(NULL, GTK_RESPONSE_ACCEPT, sel);
}

static void Select_config(gchar *title, GCallback on_ok)
{
	GKeyFile *kf;
	gchar **groups;
	gchar **g;
	GtkBuilderCScope *scope;
	GtkBuilder *builder;
	GtkWidget *dialog;
	GtkWidget *Liste;
	GtkStringList *string_list;

	kf = get_key_file();
	groups = g_key_file_get_groups(kf, NULL);

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

	dialog = GTK_WIDGET(gtk_builder_get_object(builder, "select_config_window"));
	Liste  = GTK_WIDGET(gtk_builder_get_object(builder, "select_config_list"));
	g_object_unref(builder);

	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(Fenetre));

	string_list = gtk_string_list_new(NULL);
	for (g = groups; *g; g++)
		gtk_string_list_append(string_list, *g);
	g_strfreev(groups);
	gtk_single_selection_set_model(
	    GTK_SINGLE_SELECTION(gtk_list_view_get_model(GTK_LIST_VIEW(Liste))),
	    G_LIST_MODEL(string_list));
	g_object_unref(string_list);

	gtk_window_present(GTK_WINDOW(dialog));
}

static void on_save_ok_clicked(GtkEditable *entry, GtkButton *btn)
{
	save_config(NULL, GTK_RESPONSE_ACCEPT, GTK_WIDGET(entry));
}

static void check_text_input_alnum(GtkEditable *editable, gchar *new_text,
                                   gint new_text_length, gint *position,
                                   gpointer user_data)
{
	check_text_input(editable, new_text, new_text_length, position, isalnum);
}

static void Save_config_file(void)
{
	GtkBuilderCScope *scope;
	GtkBuilder *builder;
	GtkWidget *dialog;

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

	dialog = GTK_WIDGET(gtk_builder_get_object(builder, "save_config_window"));
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(Fenetre));
	gtk_window_present(GTK_WINDOW(dialog));
}

static void really_save_config(gpointer _unused, gint id, gconstpointer data)
{
	if(id == GTK_RESPONSE_ACCEPT)
	{
		GKeyFile *kf = get_key_file();
		Copy_configuration(kf, (const gchar *)data);
		if (!save_key_file())
			return;
		g_autofree gchar *msg = g_strdup_printf(_("Configuration [%s] saved\n"), (const gchar *)data);
		show_message(msg, MSG_WRN);
	}
	else
		Save_config_file();
}

static void on_overwrite_response(GObject *source, GAsyncResult *result, gpointer data)
{
	GtkAlertDialog *alert = GTK_ALERT_DIALOG(source);
	GError *error = NULL;
	int response = gtk_alert_dialog_choose_finish(alert, result, &error);
	if (!error && response == 0)
		really_save_config(NULL, GTK_RESPONSE_ACCEPT, data);
	g_clear_error(&error);
	g_free(data);
}

static void save_config(gpointer _unused, gint id, GtkWidget *edit)
{
	if(id == GTK_RESPONSE_ACCEPT)
	{
		const gchar *config_name;
		g_autofree gchar *alert_msg;
		GtkAlertDialog *alert;
		static const char * const buttons[] = { "_Yes", "_Cancel", NULL };

		config_name = gtk_editable_get_text(GTK_EDITABLE(edit));

		if (g_key_file_has_group(get_key_file(), config_name))
		{
			alert_msg = g_strdup_printf(
			    _("Section [%s] already exists.\n\nDo you want to overwrite it ?"),
			    config_name);
			alert = gtk_alert_dialog_new("%s", alert_msg);
			gtk_alert_dialog_set_modal(alert, TRUE);
			gtk_alert_dialog_set_buttons(alert, buttons);
			gtk_alert_dialog_set_default_button(alert, 0);
			gtk_alert_dialog_set_cancel_button(alert, 1);

			gtk_alert_dialog_choose(alert, GTK_WINDOW(Fenetre), NULL,
			                        on_overwrite_response, g_strdup(config_name));
			g_object_unref(alert);
		}
		else
			really_save_config(NULL, GTK_RESPONSE_ACCEPT, (gconstpointer)config_name);
	}
}

static void load_config(gpointer _unused, gint id, GtkSingleSelection *selection)
{
	gchar *message;

	if(id == GTK_RESPONSE_ACCEPT)
	{
		GtkStringObject *obj = GTK_STRING_OBJECT(gtk_single_selection_get_selected_item(selection));
		if(obj)
		{
			const gchar *txt = gtk_string_object_get_string(obj);
			Load_configuration_from_file(txt);
			Verify_configuration();
			Config_port();
			ConfigFlags();

			message = get_port_string();
			Set_status_message(message);
			Set_window_title(message);
			g_free(message);
		}
	}
}

static void delete_config(gpointer _unused, gint id, GtkSingleSelection *selection)
{
	if(id == GTK_RESPONSE_ACCEPT)
	{
		GtkStringObject *obj = GTK_STRING_OBJECT(gtk_single_selection_get_selected_item(selection));
		if(obj)
		{
			const gchar *txt = gtk_string_object_get_string(obj);
			GError *err = NULL;
			if (!g_key_file_remove_group(get_key_file(), txt, &err))
			{
				show_message(_("Cannot delete section!"), MSG_ERR);
				g_clear_error(&err);
			}
			else
				save_key_file();
		}
	}
}

void select_config_callback(GSimpleAction *action, GVariant *param, gpointer data)
{
	Select_config(_("Load configuration"), G_CALLBACK(on_load_ok_clicked));
}

void save_config_callback(GSimpleAction *action, GVariant *param, gpointer data)
{
	Save_config_file();
}

void delete_config_callback(GSimpleAction *action, GVariant *param, gpointer data)
{
	Select_config(_("Delete configuration"), G_CALLBACK(on_delete_ok_clicked));
}
