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

static GtkEditable   *Entry;
static GtkEditable   *port_entry;
static GtkEditable   *baud_entry;
static GtkDropDown   *port_dd;
static GtkDropDown   *baud_dd;
static GtkDropDown   *parity_dd;
static GtkDropDown   *bits_dd;
static GtkDropDown   *stopbits_dd;
static GtkDropDown   *flow_dd;
static GtkSpinButton *delay_spin;
static GtkCheckButton *wait_char_check;
static GtkSpinButton *rts_before_spin;
static GtkSpinButton *rts_after_spin;
static GtkExpander   *Expander;

static gboolean dd_is_other(GtkDropDown *dd)
{
	guint n = g_list_model_get_n_items(gtk_drop_down_get_model(dd));
	return n > 0 && gtk_drop_down_get_selected(dd) == n - 1;
}

static void apply_dd_custom_state(GtkDropDown *dd)
{
	GtkEditable *entry = (dd == port_dd) ? port_entry : baud_entry;
	gboolean custom = dd_is_other(dd);
	if (custom) {
		gtk_expander_set_expanded(Expander, TRUE);
		gtk_widget_grab_focus(GTK_WIDGET(entry));
	} else {
		GtkStringObject *item = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(dd));
		if (item)
			gtk_editable_set_text(entry,
			                      gtk_string_object_get_string(item));
	}
	gtk_editable_set_editable(entry, custom);
	gtk_widget_set_sensitive(GTK_WIDGET(entry), custom);
}

static void on_dd_changed(GObject *obj, GParamSpec *pspec G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	apply_dd_custom_state(GTK_DROP_DOWN(obj));
}

static void setup_dd(GtkDropDown *dd, GtkStringList *model, guint index,
                     const char *custom_text)
{
	GtkEditable *entry = (dd == port_dd) ? port_entry : baud_entry;

	gtk_drop_down_set_model(dd, G_LIST_MODEL(model));
	g_object_unref(model);
	gtk_drop_down_set_selected(dd, index);

	if (dd_is_other(dd))
		gtk_editable_set_text(entry, custom_text);
	apply_dd_custom_state(dd);
}

/* missing_port_out must not be NULL; *missing_port_out is set to the
 * configured port name when the port is gone from the system (caller
 * must g_free it), or NULL otherwise. */
static GtkStringList *build_port_model(guint *port_index_out,
	                                   gchar **missing_port_out)
{
	GtkStringList *model;
	GPtrArray     *ports;
	guint          idx;

	g_return_val_if_fail(missing_port_out != NULL, NULL);
	*missing_port_out = NULL;

	ports = serial_find_ports();

	/* Determine which entry to pre-select:
	 *  - port found in discovered list      → use its index
	 *  - port not in list but node exists   → "Other..." (explicit custom)
	 *  - port configured but gone from sys  → warn, fall back to first port (0)
	 *  - nothing configured                 → first port (0) */
	*port_index_out = 0;
	if (config.port[0] != '\0')
	{
		if (g_ptr_array_find_with_equal_func(ports, config.port,
		                                     g_str_equal, &idx))
			*port_index_out = idx;
		else if (g_file_test(config.port, G_FILE_TEST_EXISTS))
			*port_index_out = ports->len; /* index of "Other..." */
		else
			/* Port is gone: fall back to first port (index 0 already set).
			 * Caller must show a warning and g_free the returned string. */
			*missing_port_out = g_strdup(config.port);
	}

	g_ptr_array_add(ports, NULL);
	model = gtk_string_list_new((const char * const *)ports->pdata);
	g_ptr_array_set_free_func(ports, g_free);
	g_ptr_array_free(ports, TRUE);

	gtk_string_list_append(model, _("Other..."));

	return model;
}

static GtkStringList *build_baud_model(guint *baud_index_out)
{
	GtkStringList *model = gtk_string_list_new(NULL);
	guint i;
	int   idx;
	gchar  buf[21]; /* UINT64_MAX - 20 digits + NUL */

	for (i = 0; i < baudrate_count; i++)
	{
		g_snprintf(buf, sizeof(buf), "%u", baudrate_list[i].baud);
		gtk_string_list_append(model, buf);
	}
	gtk_string_list_append(model, _("Custom..."));

	/* Select matching baud rate, or "Custom..." if not in list */
	idx = baudrate_find_index(config.vitesse);
	*baud_index_out = (idx >= 0) ? (guint)idx : baudrate_count;
	return model;
}

static void on_port_ok_clicked(GtkButton *btn, gpointer data G_GNUC_UNUSED)
{
	Lis_Config();
	gtk_window_destroy(GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(btn))));
}

static void check_baud_input(GtkEditable *editable, gchar *new_text,
                             gint new_text_length, gint *position,
                             gpointer user_data G_GNUC_UNUSED)
{
	check_text_input(editable, new_text, new_text_length, position, isdigit);
}

static gint Grise_Degrise(GtkWidget *bouton, gpointer data G_GNUC_UNUSED)
{
	gboolean active = gtk_check_button_get_active(GTK_CHECK_BUTTON(bouton));

	gtk_widget_set_sensitive(GTK_WIDGET(Entry), active);
	gtk_widget_set_sensitive(GTK_WIDGET(delay_spin), !active);

	return FALSE;
}

void Config_Port_Fenetre(GSimpleAction *action G_GNUC_UNUSED,
                         GVariant *param G_GNUC_UNUSED,
                         gpointer data G_GNUC_UNUSED)
{
	GtkBuilder       *builder;
	GtkWindow        *Dialogue;
	GtkBuilderCScope *scope;
	GtkStringList    *m;
	gchar            *missing_port = NULL;
	guint            idx;
	gchar            baud_buf[21]; /* UINT64_MAX = 20 digits + NUL */

	scope = GTK_BUILDER_CSCOPE(gtk_builder_cscope_new());
	gtk_builder_cscope_add_callback_symbols(scope,
	    "on_port_ok_clicked",  G_CALLBACK(on_port_ok_clicked),
	    "gtk_window_destroy",  G_CALLBACK(gtk_window_destroy),
	    "check_baud_input",    G_CALLBACK(check_baud_input),
	    "Grise_Degrise",       G_CALLBACK(Grise_Degrise),
	    "on_port_dd_changed",  G_CALLBACK(on_dd_changed),
	    "on_baud_dd_changed",  G_CALLBACK(on_dd_changed),
	    NULL);
	builder = gtk_builder_new();
	gtk_builder_set_scope(builder, GTK_BUILDER_SCOPE(scope));
	g_object_unref(scope);
	gtk_builder_add_from_resource(builder, "/org/gtk/gtkterm/port_config_dialog.ui", NULL);

	Dialogue        = GTK_WINDOW(gtk_builder_get_object(builder, "port_config_window"));
	Expander        = GTK_EXPANDER(gtk_builder_get_object(builder, "port_expander"));
	port_dd         = GTK_DROP_DOWN(gtk_builder_get_object(builder, "port_port_dd"));
	baud_dd         = GTK_DROP_DOWN(gtk_builder_get_object(builder, "port_baud_dd"));
	port_entry      = GTK_EDITABLE(gtk_builder_get_object(builder, "port_custom_port_entry"));
	baud_entry      = GTK_EDITABLE(gtk_builder_get_object(builder, "port_custom_baud_entry"));
	parity_dd       = GTK_DROP_DOWN(gtk_builder_get_object(builder, "port_parity_dd"));
	bits_dd         = GTK_DROP_DOWN(gtk_builder_get_object(builder, "port_bits_dd"));
	stopbits_dd     = GTK_DROP_DOWN(gtk_builder_get_object(builder, "port_stopbits_dd"));
	flow_dd         = GTK_DROP_DOWN(gtk_builder_get_object(builder, "port_flow_dd"));
	delay_spin      = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "port_delay_spin"));
	wait_char_check = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "port_wait_char_check"));
	Entry           = GTK_EDITABLE(gtk_builder_get_object(builder, "port_wait_char_entry"));
	rts_before_spin = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "port_rts_before_spin"));
	rts_after_spin  = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "port_rts_after_spin"));

	g_object_unref(builder);

	gtk_window_set_transient_for(Dialogue, Fenetre);

	/* Set initial values for static dropdowns and spin buttons */
	gtk_drop_down_set_selected(parity_dd,
	    (config.parite >= 0 && config.parite <= 2) ? (guint)config.parite : 0);
	gtk_drop_down_set_selected(bits_dd,
	    (config.bits >= 5 && config.bits <= 8) ? (guint)(config.bits - 5) : 3);
	gtk_drop_down_set_selected(stopbits_dd,
	    (config.stops == 1 || config.stops == 2) ? (guint)(config.stops - 1) : 0);
	{
		static const guint flow_to_dd[] = {0, 2, 1, 3};
		gtk_drop_down_set_selected(flow_dd,
		    config.flux < 4 ? flow_to_dd[config.flux] : 0);
	}
	gtk_spin_button_set_value(delay_spin, (gfloat)config.delai);
	gtk_spin_button_set_value(rts_before_spin,
	                          (gfloat)config.rs485_rts_time_before_transmit);
	gtk_spin_button_set_value(rts_after_spin,
	                          (gfloat)config.rs485_rts_time_after_transmit);

	if (config.car != -1)
	{
		gtk_editable_set_text(Entry, &(config.car));
		gtk_check_button_set_active(wait_char_check, TRUE);
	}

	/* Build port and baud dropdowns dynamically */
	m = build_port_model(&idx, &missing_port);
	setup_dd(port_dd, m, idx, config.port);

	m = build_baud_model(&idx);
	g_snprintf(baud_buf, sizeof(baud_buf), "%u", config.vitesse);
	setup_dd(baud_dd, m, idx, baud_buf);

	gtk_window_present(Dialogue);

	if (missing_port != NULL)
	{
		show_messagef(MSG_WRN,
		    _("Port '%s' not found. Select a different port."),
		    missing_port);
		g_free(missing_port);
	}
}

void Lis_Config(void)
{
	struct configuration_port prev_config = config;

	g_strlcpy(config.port,
	          gtk_editable_get_text(port_entry),
	          sizeof(config.port));

	config.vitesse = (guint)atoi(gtk_editable_get_text(baud_entry));

	config.bits = (gint)gtk_drop_down_get_selected(bits_dd) + 5;

	config.delai = gtk_spin_button_get_value_as_int(delay_spin);
	config.rs485_rts_time_before_transmit = gtk_spin_button_get_value_as_int(rts_before_spin);
	config.rs485_rts_time_after_transmit = gtk_spin_button_get_value_as_int(rts_after_spin);

	config.parite = (gint)gtk_drop_down_get_selected(parity_dd);

	config.stops = (gint)gtk_drop_down_get_selected(stopbits_dd) + 1;

	{
		static const gint flow_map[] = {0, 2, 1, 3};
		guint fi = gtk_drop_down_get_selected(flow_dd);
		config.flux = (fi < 4) ? flow_map[fi] : 0;
	}

	if (gtk_check_button_get_active(wait_char_check))
	{
		config.car = *gtk_editable_get_text(Entry);
		config.delai = 0;
	}
	else
		config.car = -1;

	if (memcmp(&config, &prev_config, sizeof(config)) != 0)
		Config_port();
	ConfigFlags();

	update_port_status();
}
