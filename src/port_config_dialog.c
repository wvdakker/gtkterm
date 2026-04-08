#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <glib/gi18n.h>

#include "serial.h"
#include "term_config.h"
#include "config_file.h"
#include "port_list.h"
#include "interface.h"
#include "port_config_dialog.h"

static GtkWidget *Entry;
static GtkWidget *port_entry;
static GtkWidget *baud_entry;
static GtkWidget *parity_dd;
static GtkWidget *bits_dd;
static GtkWidget *stopbits_dd;
static GtkWidget *flow_dd;
static GtkWidget *delay_spin;
static GtkWidget *wait_char_check;
static GtkWidget *rts_before_spin;
static GtkWidget *rts_after_spin;
static GtkWidget *Expander;

static void apply_dd_custom_state(GtkDropDown *dd, GtkWidget *entry)
{
	guint n    = g_list_model_get_n_items(G_LIST_MODEL(gtk_drop_down_get_model(dd)));
	guint sel  = gtk_drop_down_get_selected(dd);
	gboolean custom = (sel == n - 1);
	if (custom) {
		gtk_expander_set_expanded(GTK_EXPANDER(Expander), TRUE);
		gtk_widget_grab_focus(entry);
	} else {
		GtkStringObject *item = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(dd));
		if (item)
			gtk_editable_set_text(GTK_EDITABLE(entry),
			                      gtk_string_object_get_string(item));
	}
	gtk_editable_set_editable(GTK_EDITABLE(entry), custom);
	gtk_widget_set_sensitive(entry, custom);
}

static void on_port_dd_changed(GObject *obj, GParamSpec *pspec, gpointer unused)
{
	apply_dd_custom_state(GTK_DROP_DOWN(obj), port_entry);
}

static void on_baud_dd_changed(GObject *obj, GParamSpec *pspec, gpointer unused)
{
	apply_dd_custom_state(GTK_DROP_DOWN(obj), baud_entry);
}

static GtkStringList *build_port_model(GPtrArray *ports, int *port_index_out)
{
	GtkStringList *model = gtk_string_list_new(NULL);
	guint model_len = 0;
	char *prev = "";
	guint i;

	*port_index_out = -1;
	for (i = 0; i < ports->len; i++)
	{
		char *curr = ports->pdata[i];
		if (strcmp(prev, curr))
		{
			if (config.port[0] != '\0' && strcmp(curr, config.port) == 0)
				*port_index_out = (int)model_len;
			gtk_string_list_append(model, curr);
			model_len++;
			prev = curr;
		}
	}

	gtk_string_list_append(model, _("Other..."));
	return model;
}

static GtkStringList *build_baud_model(void)
{
	unsigned int i;
	GtkStringList *model = gtk_string_list_new(NULL);
	for (i = 0; i < baudrate_count; i++)
	{
		gchar *s = g_strdup_printf("%u", baudrate_list[i].baud);
		gtk_string_list_append(model, s);
		g_free(s);
	}
	gtk_string_list_append(model, _("Custom..."));
	return model;
}

static void on_port_ok_clicked(GtkButton *btn, gpointer unused)
{
	Lis_Config();
	gtk_window_destroy(GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(btn))));
}

static void check_baud_input(GtkEditable *editable, gchar *new_text,
                             gint new_text_length, gint *position,
                             gpointer user_data)
{
	check_text_input(editable, new_text, new_text_length, position, isdigit);
}

static gint Grise_Degrise(GtkWidget *bouton, gpointer unused)
{
	if(gtk_check_button_get_active(GTK_CHECK_BUTTON(bouton)))
	{
		gtk_widget_set_sensitive(Entry, TRUE);
		gtk_widget_set_sensitive(delay_spin, FALSE);
	}
	else
	{
		gtk_widget_set_sensitive(Entry, FALSE);
		gtk_widget_set_sensitive(delay_spin, TRUE);
	}
	return FALSE;
}

void Config_Port_Fenetre(GSimpleAction *action, GVariant *param, gpointer data)
{
	GtkBuilder *builder;
	GtkWidget *Dialogue;
	GtkWidget *port_dd, *baud_dd;
	GPtrArray *ports;
	int speed_index;
	int port_index;

	gchar *no_ports_msg = NULL;
	ports = serial_find_ports(&no_ports_msg);
	if (no_ports_msg)
	{
		show_message(no_ports_msg, MSG_WRN);
		g_free(no_ports_msg);
	}

	GtkBuilderCScope *scope = GTK_BUILDER_CSCOPE(gtk_builder_cscope_new());
	gtk_builder_cscope_add_callback_symbols(scope,
	    "on_port_ok_clicked",  G_CALLBACK(on_port_ok_clicked),
	    "gtk_window_destroy",  G_CALLBACK(gtk_window_destroy),
	    "check_baud_input",    G_CALLBACK(check_baud_input),
	    "Grise_Degrise",       G_CALLBACK(Grise_Degrise),
	    "on_port_dd_changed",  G_CALLBACK(on_port_dd_changed),
	    "on_baud_dd_changed",  G_CALLBACK(on_baud_dd_changed),
	    NULL);
	builder = gtk_builder_new();
	gtk_builder_set_scope(builder, GTK_BUILDER_SCOPE(scope));
	g_object_unref(scope);
	gtk_builder_add_from_resource(builder, "/org/gtk/gtkterm/port_config_dialog.ui", NULL);

	Dialogue    = GTK_WIDGET(gtk_builder_get_object(builder, "port_config_window"));
	Expander    = GTK_WIDGET(gtk_builder_get_object(builder, "port_expander"));	port_dd         = GTK_WIDGET(gtk_builder_get_object(builder, "port_port_dd"));
	baud_dd         = GTK_WIDGET(gtk_builder_get_object(builder, "port_baud_dd"));
	port_entry    = GTK_WIDGET(gtk_builder_get_object(builder, "port_custom_port_entry"));
	baud_entry    = GTK_WIDGET(gtk_builder_get_object(builder, "port_custom_baud_entry"));
	parity_dd     = GTK_WIDGET(gtk_builder_get_object(builder, "port_parity_dd"));
	bits_dd       = GTK_WIDGET(gtk_builder_get_object(builder, "port_bits_dd"));
	stopbits_dd   = GTK_WIDGET(gtk_builder_get_object(builder, "port_stopbits_dd"));
	flow_dd       = GTK_WIDGET(gtk_builder_get_object(builder, "port_flow_dd"));
	delay_spin    = GTK_WIDGET(gtk_builder_get_object(builder, "port_delay_spin"));
	wait_char_check = GTK_WIDGET(gtk_builder_get_object(builder, "port_wait_char_check"));
	Entry         = GTK_WIDGET(gtk_builder_get_object(builder, "port_wait_char_entry"));
	rts_before_spin = GTK_WIDGET(gtk_builder_get_object(builder, "port_rts_before_spin"));
	rts_after_spin  = GTK_WIDGET(gtk_builder_get_object(builder, "port_rts_after_spin"));

	g_object_unref(builder);

	gtk_window_set_transient_for(GTK_WINDOW(Dialogue), GTK_WINDOW(Fenetre));

	/* Set initial values for static dropdowns and spin buttons */
	gtk_drop_down_set_selected(GTK_DROP_DOWN(parity_dd),
	    (config.parite >= 0 && config.parite <= 2) ? (guint)config.parite : 0);
	gtk_drop_down_set_selected(GTK_DROP_DOWN(bits_dd),
	    (config.bits >= 5 && config.bits <= 8) ? (guint)(config.bits - 5) : 3);
	gtk_drop_down_set_selected(GTK_DROP_DOWN(stopbits_dd),
	    (config.stops == 1 || config.stops == 2) ? (guint)(config.stops - 1) : 0);
	{
		static const guint flow_to_dd[] = {0, 2, 1, 3};
		gtk_drop_down_set_selected(GTK_DROP_DOWN(flow_dd),
		    config.flux < 4 ? flow_to_dd[config.flux] : 0);
	}
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(delay_spin), (gfloat)config.delai);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(rts_before_spin),
	                          (gfloat)config.rs485_rts_time_before_transmit);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(rts_after_spin),
	                          (gfloat)config.rs485_rts_time_after_transmit);

	if (config.car != -1)
	{
		gtk_editable_set_text(GTK_EDITABLE(Entry), &(config.car));
		gtk_check_button_set_active(GTK_CHECK_BUTTON(wait_char_check), TRUE);
	}


	/* Build port dropdown dynamically */
	{
		GtkStringList *port_model = build_port_model(ports, &port_index);
		for (guint j = 0; j < ports->len; j++)
			g_free(ports->pdata[j]);
		g_ptr_array_free(ports, TRUE);
		gtk_drop_down_set_model(GTK_DROP_DOWN(port_dd), G_LIST_MODEL(port_model));
		g_object_unref(port_model);
		gtk_drop_down_set_selected(GTK_DROP_DOWN(port_dd),
		    port_index >= 0 ? (guint)port_index
		                    : g_list_model_get_n_items(G_LIST_MODEL(gtk_drop_down_get_model(GTK_DROP_DOWN(port_dd)))) - 1);
	}

	/* Build baud rate dropdown dynamically */
	{
		GtkStringList *baud_model;
		if (!config.vitesse)
			config.vitesse = DEFAULT_SPEED;
		speed_index = baudrate_find_index(config.vitesse);
		baud_model = build_baud_model();
		gtk_drop_down_set_model(GTK_DROP_DOWN(baud_dd), G_LIST_MODEL(baud_model));
		g_object_unref(baud_model);
		gtk_drop_down_set_selected(GTK_DROP_DOWN(baud_dd),
		    speed_index >= 0 ? (guint)speed_index : baudrate_count);
	}

	/* Wire port dropdown → custom port entry */
	{
		gboolean custom = (port_index < 0);
		if (config.port[0] != '\0')
			gtk_editable_set_text(GTK_EDITABLE(port_entry), config.port);
		gtk_editable_set_editable(GTK_EDITABLE(port_entry), custom);
		gtk_widget_set_sensitive(port_entry, custom);
		if (custom)
			gtk_expander_set_expanded(GTK_EXPANDER(Expander), TRUE);
	}

	/* Wire baud dropdown → custom baud entry */
	{
		gboolean custom = (speed_index < 0);
		gchar *string;
		string = g_strdup_printf("%u", config.vitesse);
		gtk_editable_set_text(GTK_EDITABLE(baud_entry), string);
		g_free(string);
		gtk_editable_set_editable(GTK_EDITABLE(baud_entry), custom);
		gtk_widget_set_sensitive(baud_entry, custom);
		if (custom)
			gtk_expander_set_expanded(GTK_EXPANDER(Expander), TRUE);
	}

	gtk_window_present(GTK_WINDOW(Dialogue));
}

void Lis_Config(void)
{

	gchar *message;

	g_strlcpy(config.port,
	          gtk_editable_get_text(GTK_EDITABLE(port_entry)),
	          sizeof(config.port));

	config.vitesse = (guint)atoi(gtk_editable_get_text(GTK_EDITABLE(baud_entry)));

	config.bits = (gint)gtk_drop_down_get_selected(GTK_DROP_DOWN(bits_dd)) + 5;

	config.delai = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(delay_spin));
	config.rs485_rts_time_before_transmit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rts_before_spin));
	config.rs485_rts_time_after_transmit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(rts_after_spin));

	config.parite = (gint)gtk_drop_down_get_selected(GTK_DROP_DOWN(parity_dd));

	config.stops = (gint)gtk_drop_down_get_selected(GTK_DROP_DOWN(stopbits_dd)) + 1;

	{
		static const gint flow_map[] = {0, 2, 1, 3};
		guint fi = gtk_drop_down_get_selected(GTK_DROP_DOWN(flow_dd));
		config.flux = (fi < 4) ? flow_map[fi] : 0;
	}

	if (gtk_check_button_get_active(GTK_CHECK_BUTTON(wait_char_check)))
	{
		config.car = *gtk_editable_get_text(GTK_EDITABLE(Entry));
		config.delai = 0;
	}
	else
		config.car = -1;

	Config_port();
	ConfigFlags();

	message = get_port_string();
	Set_status_message(message);
	Set_window_title(message);
	g_free(message);
}
