/***********************************************************************/
/* interface.c                                                         */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Functions for the management of the GUI for the main window    */
/*      Ported to GTK4                                                 */
/*                                                                     */
/***********************************************************************/

#include "config.h"

#include <gtk/gtk.h>
#ifdef HAVE_LINUX_TERMIOS_H
# include <linux/termios.h>	/* For control signals */
# define NO_TERMIOS		/* Conflicts with <termios.h> */
#endif
#include <vte/vte.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "term_config.h"
#include "port_config_dialog.h"
#include "terminal_config.h"
#include "config_dialog.h"
#include "files.h"
#include "search.h"
#include "serial.h"
#include "interface.h"
#include "buffer.h"
#include "macros.h"
#include "auto_config.h"
#include "logging.h"
#include "device_monitor.h"

#include <glib/gi18n.h>
#include "config_file.h"

GtkWidget *StatusBar;
GtkWidget *signals[6];
static GtkWidget *Hex_Box;
GtkWidget *searchBar;
GtkWidget *Fenetre;
static GtkWidget *popup_menu;
static GtkApplication *main_app;
GtkWidget *display = NULL;
static gulong got_input_handler_id = 0;
/* Nesting counter: >0 means the commit handler is blocked.
 * Incremented by set_view(), decremented by the 50ms unblock timeout.
 * Using a counter (not a bool) ensures rapid set_view() calls don't
 * unblock prematurely. */
static guint pending_reenable = 0;

GList *hex_history = NULL;
GList *current_hex = NULL;

/* Variables for hexadecimal display */
static guint bytes_per_line = 16;
static gchar blank_data[128];
static guint total_bytes;
static gboolean show_index = FALSE;
guint virt_col_pos = 0;

/* Forward declarations */
gboolean pop_message(gpointer user_data);
static void Send_Hexadecimal(GtkWidget *widget, gpointer pointer);
void update_hex_history(GtkWidget *widget);
void set_saved_data(GtkWidget *widget, gboolean direction);
void show_control_signals(int stat);
void update_copy_sensivity(VteTerminal *terminal, gpointer data);
static void Got_Input(VteTerminal *widget, gchar *text, guint length, gpointer ptr);
gboolean control_signals_read(gpointer user_data);
gboolean on_hex_key_pressed(GtkEventControllerKey *ctrl,
                                    guint keyval, guint keycode,
                                    GdkModifierType state, gpointer user_data);

static void file_exit_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	g_application_quit(G_APPLICATION(main_app));
}

static void clear_screen_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	clear_buffer();
}

static void clear_scrollback_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	clear_scrollback();
}

static void edit_copy_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	vte_terminal_copy_clipboard_format(VTE_TERMINAL(display), VTE_FORMAT_TEXT);
}

static void edit_paste_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	vte_terminal_paste_clipboard(VTE_TERMINAL(display));
}

static void edit_find_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	if (gtk_widget_is_visible(searchBar))
		search_bar_hide(searchBar);
	else
		search_bar_show(searchBar);
}

static void edit_select_all_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	vte_terminal_select_all(VTE_TERMINAL(display));
}

static void signals_send_break_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	sendbreak();
	Put_temp_message(_("Break signal sent!"), 800);
}

static void signals_toggle_DTR_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	Set_signals(0);
}

static void signals_toggle_RTS_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	Set_signals(1);
}

static void on_signal_label_clicked(GtkGestureClick *gesture, int n_press,
                                    double x, double y, gpointer data)
{
	int param = GPOINTER_TO_INT(data);
	/* RTS is managed by the driver under HW flow control (flux==2) or RS485 (flux==3) */
	if (param == 1 && (config.flux == 2 || config.flux == 3))
		return;
	Set_signals(param);
}

static void on_dtr_clicked(GtkGestureClick *gesture, int n_press,
                           double x, double y, gpointer unused)
{
	on_signal_label_clicked(gesture, n_press, x, y, GINT_TO_POINTER(0));
}

static void on_rts_clicked(GtkGestureClick *gesture, int n_press,
                           double x, double y, gpointer unused)
{
	on_signal_label_clicked(gesture, n_press, x, y, GINT_TO_POINTER(1));
}

static void signals_close_port_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	interface_close_port();
}

static void signals_open_port_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	interface_open_port();
}

static void help_shortcuts_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	GtkBuilder *builder = gtk_builder_new_from_resource("/org/gtk/gtkterm/shortcuts_window.ui");
	GtkWidget *win = GTK_WIDGET(gtk_builder_get_object(builder, "shortcuts_window"));
	gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(Fenetre));
	gtk_window_present(GTK_WINDOW(win));
	g_object_unref(builder);
}

static void help_about_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	static const gchar *authors[] = {
		"Julien Schimtt",
		"Zach Davis",
		"Florian Euchner",
		"Stephan Enderlein",
		"Kevin Picot",
		NULL
	};
	const gchar *comments_program = _("GTKTerm is a simple GTK+ terminal used to communicate with the serial port.");

	gchar *comments = g_strdup_printf("%s\n\n%s", RELEASE_DATE, comments_program);
	GdkTexture *logo_texture = gdk_texture_new_from_resource("/org/gtk/gtkterm/icons/256x256/apps/gtkterm.png");

	gtk_show_about_dialog(GTK_WINDOW(Fenetre),
	                      "program-name", "GTKTerm",
	                      "version", VERSION,
	                      "comments", comments,
	                      "copyright", "Copyright © Julien Schimtt",
	                      "authors", authors,
	                      "website", "https://github.com/Jeija/gtkterm",
	                      "website-label", "https://github.com/Jeija/gtkterm",
	                      "license-type", GTK_LICENSE_LGPL_3_0,
	                      "logo", logo_texture,
	                      NULL);
	if (logo_texture) g_object_unref(logo_texture);
	g_free(comments);
}

/* Toggle / radio change-state callbacks */

static void echo_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	config.echo = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
}

static void autoreconnect_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	config.autoreconnect_enabled = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
}

static void crlfauto_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	config.crlfauto = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
}

static void esc_clear_screen_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	config.esc_clear_screen = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
}

static void timestamp_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	config.timestamp = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
}

static void view_index_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	show_index = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
	set_view(HEXADECIMAL_VIEW);
}

static void view_send_hex_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	g_simple_action_set_state(a, s);
	gtk_widget_set_visible(Hex_Box, g_variant_get_boolean(s));
}

static void view_mode_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	const gchar *mode = g_variant_get_string(s, NULL);
	g_simple_action_set_state(a, s);
	set_view(g_str_equal(mode, "hex") ? HEXADECIMAL_VIEW : ASCII_VIEW);
}

static void hex_chars_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	const gchar *val = g_variant_get_string(s, NULL);
	g_simple_action_set_state(a, s);
	bytes_per_line = atoi(val);
	set_view(HEXADECIMAL_VIEW);
}

/* Snapshot of accelerators read from the application after the UI is loaded.
 * Keys and values are both heap-allocated strings owned by the GHashTable. */
static GHashTable *accel_snapshot = NULL; /* action → gchar** (NULL-terminated) */

static void snapshot_accels(void)
{
	gchar **actions;
	gchar **a;
	accel_snapshot = g_hash_table_new_full(g_str_hash, g_str_equal,
	                                       g_free, (GDestroyNotify)g_strfreev);
	actions = gtk_application_list_action_descriptions(main_app);
	for (a = actions; *a; a++) {
		gchar **accels = gtk_application_get_accels_for_action(main_app, *a);
		if (accels && accels[0])
			g_hash_table_insert(accel_snapshot, g_strdup(*a), accels);
		else
			g_strfreev(accels);
	}
	g_strfreev(actions);
}

static void apply_hotkeys_disabled(gboolean disabled)
{
	GHashTableIter iter;
	gpointer key, val;
	/* Lazy snapshot: taken on first call, after GTK has processed all
	 * <attribute name="accel"> entries from the menu model. */
	if (!accel_snapshot)
		snapshot_accels();
	set_macros_shortcuts_enabled(!disabled);
	g_hash_table_iter_init(&iter, accel_snapshot);
	while (g_hash_table_iter_next(&iter, &key, &val)) {
		if (disabled) {
			const char *empty[] = { NULL };
			gtk_application_set_accels_for_action(main_app, key, empty);
		} else {
			gtk_application_set_accels_for_action(main_app, key, val);
		}
	}
}

static void disable_hotkeys_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	config.disable_hotkeys = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
	apply_hotkeys_disabled(config.disable_hotkeys);
}

/* Normal action entries table */
static const GActionEntry app_actions[] = {
	{ "file-exit",          file_exit_cb },
	{ "clear-screen",       clear_screen_cb },
	{ "clear-scrollback",   clear_scrollback_cb },
	{ "send-raw-file",      send_raw_file },
	{ "save-raw-file",      save_raw_file },
	{ "save-ascii-file",    save_ascii_file },
	{ "edit-copy",          edit_copy_cb },
	{ "edit-paste",         edit_paste_cb },
	{ "edit-find",          edit_find_cb },
	{ "edit-select-all",    edit_select_all_cb },
	{ "log-to-file",        logging_start },
	{ "log-pause-resume",   logging_pause_resume },
	{ "log-stop",           logging_stop },
	{ "log-clear",          logging_clear },
	{ "config-port",        Config_Port_Fenetre },
	{ "config-terminal",    Config_Terminal },
	{ "macros",             Config_macros },
	{ "select-config",      select_config_callback },
	{ "save-config",        save_config_callback },
	{ "delete-config",      delete_config_callback },
	{ "signals-send-break", signals_send_break_cb },
	{ "signals-open-port",  signals_open_port_cb },
	{ "signals-close-port", signals_close_port_cb },
	{ "signals-dtr",        signals_toggle_DTR_cb },
	{ "signals-rts",        signals_toggle_RTS_cb },
	{ "help-shortcuts",     help_shortcuts_cb },
	{ "help-about",         help_about_cb },
	/* Stateful toggles (NULL activate, type, initial state, change-state) */
	{ "local-echo",         NULL, NULL, "false", echo_change_state },
	{ "autoreconnect",      NULL, NULL, "false", autoreconnect_change_state },
	{ "crlfauto",           NULL, NULL, "false", crlfauto_change_state },
	{ "esc-clear-screen",   NULL, NULL, "false", esc_clear_screen_change_state },
	{ "timestamp",          NULL, NULL, "false", timestamp_change_state },
	{ "view-index",         NULL, NULL, "false", view_index_change_state },
	{ "view-send-hex",      NULL, NULL, "false", view_send_hex_change_state },
	/* Stateful radio actions */
	{ "view-mode",          NULL, "s",  "'ascii'", view_mode_change_state },
	{ "hex-chars",          NULL, "s",  "'16'",    hex_chars_change_state },
	/* Simple action used only for enabling/disabling the hex-chars submenu */
	{ "hex-chars-submenu",  NULL },
	{ "disable-hotkeys",    NULL, NULL, "false", disable_hotkeys_change_state },
};


/* Exported helpers that update action state */

static void set_bool_action(const char *name, gboolean value)
{
	GAction *action = g_action_map_lookup_action(G_ACTION_MAP(main_app), name);
	if (action)
		g_simple_action_set_state(G_SIMPLE_ACTION(action),
		                          g_variant_new_boolean(value));
}

static void set_action_enabled(const char *name, gboolean enabled)
{
	GAction *action = g_action_map_lookup_action(G_ACTION_MAP(main_app), name);
	if (action)
		g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);
}

static void set_string_action(const char *name, const char *value)
{
	GAction *action = g_action_map_lookup_action(G_ACTION_MAP(main_app), name);
	if (action)
		g_simple_action_set_state(G_SIMPLE_ACTION(action),
		                          g_variant_new_string(value));
}

void Set_local_echo(gboolean v)            { config.echo                  = v; set_bool_action("local-echo",       v); }
void Set_crlfauto(gboolean v)              { config.crlfauto              = v; set_bool_action("crlfauto",         v); }
void Set_autoreconnect_enabled(gboolean v) { config.autoreconnect_enabled = v; set_bool_action("autoreconnect",    v); }
void Set_esc_clear_screen(gboolean v)      { config.esc_clear_screen      = v; set_bool_action("esc-clear-screen", v); }
void Set_timestamp(gboolean v)             { config.timestamp             = v; set_bool_action("timestamp",        v); }

void Set_hotkeys_disabled(gboolean disabled)
{
	config.disable_hotkeys = disabled;
	set_bool_action("disable-hotkeys", disabled);
	apply_hotkeys_disabled(disabled);
}

static gboolean reenable_commit_cb(gpointer data)
{
	if (--pending_reenable == 0 && got_input_handler_id && display)
		g_signal_handler_unblock(display, got_input_handler_id);
	return G_SOURCE_REMOVE;
}

void set_view(guint type)
{
	/* Block the commit→serial path for the duration of the clear+replay.
	 * vte_terminal_set_input_enabled() does NOT suppress commit emissions
	 * from data fed via vte_terminal_feed() — VTE still responds to escape
	 * sequences (DSR, XTGETTCAP, etc.) in the replayed buffer regardless.
	 * g_signal_handler_block() silences Got_Input at the signal level.
	 * A 50ms timeout (>> VTE_DISPLAY_TIMEOUT=1ms) guarantees the unblock
	 * fires after all of VTE's async processing rounds. A counter handles
	 * rapid back-to-back set_view() calls safely. */
	if (pending_reenable == 0 && got_input_handler_id && display)
		g_signal_handler_block(display, got_input_handler_id);
	pending_reenable++;

	clear_display();
	set_clear_func(clear_display);
	switch (type)
	{
	case ASCII_VIEW:
		set_string_action("view-mode", "ascii");
		set_action_enabled("view-index",        FALSE);
		set_action_enabled("hex-chars",         FALSE);
		set_action_enabled("hex-chars-submenu", FALSE);
		total_bytes = 0;
		set_display_func(put_text);
		break;
	case HEXADECIMAL_VIEW:
		set_string_action("view-mode", "hex");
		set_action_enabled("view-index",        TRUE);
		set_action_enabled("hex-chars",         TRUE);
		set_action_enabled("hex-chars-submenu", TRUE);
		total_bytes = 0;
		virt_col_pos = 0;
		set_display_func(put_hexadecimal);
		break;
	default:
		set_display_func(NULL);
	}
	write_buffer();

	g_timeout_add(50, reenable_commit_cb, NULL);
}

void toggle_logging_pause_resume(gboolean currentlyLogging)
{
	/* Label changes are not straightforward with GMenu; just no-op */
}

void toggle_logging_sensitivity(gboolean currentlyLogging)
{
	set_action_enabled("log-to-file",      !currentlyLogging);
	set_action_enabled("log-pause-resume",  currentlyLogging);
	set_action_enabled("log-stop",          currentlyLogging);
	set_action_enabled("log-clear",         currentlyLogging);
}

/* Right-click popup */

void on_right_click_pressed(GtkGestureClick *gesture, int n_press,
                                   double x, double y, gpointer data)
{
	GtkPopoverMenu *pmenu = GTK_POPOVER_MENU(data);
	GdkRectangle rect = { (int)x, (int)y, 1, 1 };
	gtk_popover_set_pointing_to(GTK_POPOVER(pmenu), &rect);
	gtk_popover_popup(GTK_POPOVER(pmenu));
}

gboolean terminal_popup_key_cb(GtkEventControllerKey *ctrl,
                                      guint keyval, guint keycode,
                                      GdkModifierType state, gpointer data)
{
	if (keyval == GDK_KEY_Menu ||
	    (keyval == GDK_KEY_F10 && (state & GDK_SHIFT_MASK)))
	{
		gtk_popover_popup(GTK_POPOVER(data));
		return TRUE;
	}
	return FALSE;
}

void interface_apply_term_config(void)
{
	if (!display)
		return;
	vte_terminal_set_size(VTE_TERMINAL(display), term_conf.rows, term_conf.columns);
	vte_terminal_set_scrollback_lines(VTE_TERMINAL(display), term_conf.scrollback);
	vte_terminal_set_color_foreground(VTE_TERMINAL(display), &term_conf.foreground_color);
	vte_terminal_set_color_background(VTE_TERMINAL(display), &term_conf.background_color);
	vte_terminal_set_cursor_shape(VTE_TERMINAL(display),
	    term_conf.block_cursor ? VTE_CURSOR_SHAPE_BLOCK : VTE_CURSOR_SHAPE_IBEAM);
	gtk_widget_queue_draw(display);
}

/* ---- Main window creation ---- */

void create_main_window(GtkApplication *app)
{
	GtkBuilderCScope *scope;
	GtkBuilder *builder;
	GtkWidget *sb_placeholder;
	GtkShortcutController *macro_ctrl;

	main_app = app;

	/* Register all actions on the application */
	g_action_map_add_action_entries(G_ACTION_MAP(app), app_actions,
	                                G_N_ELEMENTS(app_actions), NULL);

	/* Ensure VteTerminal's GLib type is registered before GtkBuilder
	 * tries to instantiate <object class="VteTerminal"> from the UI file.
	 * Without this, gtk_builder_get_object(builder, "display") returns NULL. */
	g_type_ensure(VTE_TYPE_TERMINAL);

	/* Load window layout and menu models from resource */
	scope = GTK_BUILDER_CSCOPE(gtk_builder_cscope_new());
	gtk_builder_cscope_add_callback_symbols(scope,
	    "g_application_quit",         G_CALLBACK(g_application_quit),
	    "Send_Hexadecimal",           G_CALLBACK(Send_Hexadecimal),
	    "on_hex_key_pressed",         G_CALLBACK(on_hex_key_pressed),
	    "on_dtr_clicked",             G_CALLBACK(on_dtr_clicked),
	    "on_rts_clicked",             G_CALLBACK(on_rts_clicked),
	    "on_right_click_pressed",     G_CALLBACK(on_right_click_pressed),
	    "terminal_popup_key_cb",      G_CALLBACK(terminal_popup_key_cb),
	    "update_copy_sensivity",      G_CALLBACK(update_copy_sensivity),
	    "gtk_widget_unparent",        G_CALLBACK(gtk_widget_unparent),
	    NULL);

	builder = gtk_builder_new();
	gtk_builder_set_scope(builder, GTK_BUILDER_SCOPE(scope));
	g_object_unref(scope);
	gtk_builder_add_from_resource(builder, "/org/gtk/gtkterm/main_window.ui", NULL);

	Fenetre = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
	gtk_window_set_application(GTK_WINDOW(Fenetre), GTK_APPLICATION(app));

	gtk_icon_theme_add_resource_path(
		gtk_icon_theme_get_for_display(gdk_display_get_default()),
		"/org/gtk/gtkterm/icons");

	Set_window_title("GTKTerm");

	/* Retrieve static widgets */
	Hex_Box         = GTK_WIDGET(gtk_builder_get_object(builder, "hex_box"));
	StatusBar       = GTK_WIDGET(gtk_builder_get_object(builder, "status_bar"));
	signals[5]      = GTK_WIDGET(gtk_builder_get_object(builder, "signal_dtr"));
	signals[4]      = GTK_WIDGET(gtk_builder_get_object(builder, "signal_rts"));
	signals[3]      = GTK_WIDGET(gtk_builder_get_object(builder, "signal_cts"));
	signals[2]      = GTK_WIDGET(gtk_builder_get_object(builder, "signal_cd"));
	signals[1]      = GTK_WIDGET(gtk_builder_get_object(builder, "signal_dsr"));
	signals[0]      = GTK_WIDGET(gtk_builder_get_object(builder, "signal_ri"));

	/* VTE terminal */
	display = GTK_WIDGET(gtk_builder_get_object(builder, "display"));

	clear_display();

	/* Search bar — inserted into its placeholder box */
	sb_placeholder = GTK_WIDGET(gtk_builder_get_object(builder, "search_bar_placeholder"));
	searchBar = search_bar_new(GTK_WINDOW(Fenetre), VTE_TERMINAL(display));
	gtk_box_append(GTK_BOX(sb_placeholder), GTK_WIDGET(searchBar));

	/* Right-click popup — constructed from model declared in the builder */
	popup_menu = GTK_WIDGET(gtk_builder_get_object(builder, "popup_menu"));
	gtk_widget_set_parent(popup_menu, GTK_WIDGET(display));

	macro_ctrl = GTK_SHORTCUT_CONTROLLER(
	                gtk_builder_get_object(builder, "macro_shortcut_ctrl"));
	g_object_unref(builder);

	set_action_enabled("edit-copy", FALSE);

	toggle_logging_sensitivity(FALSE);

	got_input_handler_id = g_signal_connect_after(GTK_WIDGET(display), "commit",
	                                              G_CALLBACK(Got_Input), NULL);

	g_timeout_add(POLL_DELAY, control_signals_read, NULL);

	install_macro_shortcut_controller(macro_ctrl);

	gtk_widget_set_visible(Fenetre, TRUE);
}

void initialize_hexadecimal_display(void)
{
	total_bytes = 0;
	memset(blank_data, ' ', 128);
	blank_data[bytes_per_line * 3 + 5] = 0;
}

void put_hexadecimal(const gchar *string, size_t size)
{
	static gchar data[128];
	static gchar data_byte[16];
	size_t i = 0;

	if (size == 0)
		return;

	while (i < size)
	{
		data[0] = 0;

		while (virt_col_pos < bytes_per_line && i < size)
		{
			guint avance = 0;
			gchar ascii[1];

			if (show_index)
			{
				if (virt_col_pos == 0)
				{
					sprintf(data, "%6u: ", total_bytes);
					vte_terminal_feed(VTE_TERMINAL(display), data, strlen(data));
				}
			}

			sprintf(data_byte, "%02X ", (guchar)string[i]);
			log_chars(data_byte, 3);
			vte_terminal_feed(VTE_TERMINAL(display), data_byte, 3);

			avance = (bytes_per_line - virt_col_pos) * 3 + virt_col_pos + 2;
			sprintf(data_byte, "%c[%uC", 27, avance);
			vte_terminal_feed(VTE_TERMINAL(display), data_byte, strlen(data_byte));

			ascii[0] = (string[i] > 0x1F) ? string[i] : '.';
			vte_terminal_feed(VTE_TERMINAL(display), ascii, 1);

			sprintf(data_byte, "%c[%uD", 27, avance + 1U);
			vte_terminal_feed(VTE_TERMINAL(display), data_byte, strlen(data_byte));

			if (virt_col_pos == bytes_per_line / 2 - 1)
				vte_terminal_feed(VTE_TERMINAL(display), "- ", strlen("- "));

			virt_col_pos++;
			i++;

			if (virt_col_pos == bytes_per_line)
			{
				vte_terminal_feed(VTE_TERMINAL(display), "\r\n", 2);
				total_bytes += virt_col_pos;
				virt_col_pos = 0;
			}
		}
	}
}

void put_text(const gchar *string, size_t size)
{
	log_chars(string, (guint)size);
	vte_terminal_feed(VTE_TERMINAL(display), string, (gssize)size);
}

gint send_serial(gchar *string, gint len)
{
	ssize_t bytes_written = Send_chars(string, len);
	if (bytes_written > 0)
	{
		if (config.echo)
			put_chars(string, (size_t)bytes_written, config.crlfauto);
	}
	return (gint)bytes_written;
}

static void Got_Input(VteTerminal *widget, gchar *text, guint length, gpointer ptr)
{
	if (config.esc_clear_screen && length >= 1 && (guchar)text[0] == 0x1b)
	{
		clear_buffer();
		clear_display();
		return;
	}
	send_serial(text, (gint)length);
}

void update_copy_sensivity(VteTerminal *terminal, gpointer data)
{
	gboolean can_copy = vte_terminal_get_has_selection(VTE_TERMINAL(terminal));
	set_action_enabled("edit-copy", can_copy);
}

void show_control_signals(int stat)
{
	gtk_widget_set_sensitive(signals[0], (stat & TIOCM_RI)  != 0);
	gtk_widget_set_sensitive(signals[1], (stat & TIOCM_DSR) != 0);
	gtk_widget_set_sensitive(signals[2], (stat & TIOCM_CD)  != 0);
	gtk_widget_set_sensitive(signals[3], (stat & TIOCM_CTS) != 0);
	/* DTR and RTS: use opacity to show on/off state; always sensitive so the
	   signal level is visible. Clicking RTS when driver-managed (HW flow or
	   RS485) is silently ignored in on_signal_label_clicked(). */
	gtk_widget_set_opacity(signals[5], (stat & TIOCM_DTR) ? 1.0 : 0.3);
	gtk_widget_set_opacity(signals[4], (stat & TIOCM_RTS) ? 1.0 : 0.3);
}

gboolean control_signals_read(gpointer user_data)
{
	int state = lis_sig();
	if (state >= 0)
		show_control_signals(state);
	return TRUE;
}

void Set_status_message(gchar *msg)
{
	gtk_label_set_text(GTK_LABEL(StatusBar), msg ? msg : "");
}

void Set_window_title(const gchar *msg)
{
	gchar *header = g_strdup_printf("GTKTerm - %s", msg);
	gtk_window_set_title(GTK_WINDOW(Fenetre), header);
	g_free(header);
}

void interface_open_port(void)
{
	gchar *message;

	Config_port();
	message = get_port_string();
	Set_status_message(message);
	Set_window_title(message);
	g_free(message);
}

void interface_close_port(void)
{
	gchar *message;

	Close_port();
	message = get_port_string();
	Set_status_message(message);
	Set_window_title(message);
	g_free(message);
}

void show_message(gint type_msg, const gchar *message)
{
	GtkAlertDialog *dialog;

	if (type_msg != MSG_ERR && type_msg != MSG_WRN)
		return;

	dialog = gtk_alert_dialog_new("%s", message);
	gtk_alert_dialog_set_modal(dialog, TRUE);
	gtk_alert_dialog_show(dialog, GTK_WINDOW(Fenetre));
	g_object_unref(dialog);
}

void show_messagef(gint type_msg, const gchar *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	gchar *msg = g_strdup_vprintf(fmt, args);
	va_end(args);
	show_message(type_msg, msg);
	g_free(msg);
}

static void Send_Hexadecimal(GtkWidget *widget, gpointer pointer)
{
	guint i;
	gchar *message, **tokens, *buff;
	guint scan_val;

	const gchar *text = gtk_editable_get_text(GTK_EDITABLE(widget));

	if (!*text)
	{
		Put_temp_message(_("0 byte(s) sent!"), 1500);
		gtk_editable_set_text(GTK_EDITABLE(widget), "");
		return;
	}

	tokens = g_strsplit_set(text, " ;", -1);
	buff = g_malloc(g_strv_length(tokens));

	for (i = 0; tokens[i] != NULL; i++)
	{
		if (sscanf(tokens[i], "%02X", &scan_val) != 1)
		{
			Put_temp_message(_("Improper formatted hex input, 0 bytes sent!"), 1500);
			g_free(buff);
			g_strfreev(tokens);
			return;
		}
		buff[i] = (gchar)scan_val;
	}

	send_serial(buff, i);
	g_free(buff);

	message = g_strdup_printf(_("%u byte(s) sent!"), i);
	update_hex_history(widget);
	Put_temp_message(message, 2000);
	g_free(message);
	gtk_editable_set_text(GTK_EDITABLE(widget), "");
	g_strfreev(tokens);
}

void Put_temp_message(const gchar *text, gint time)
{
	gtk_label_set_text(GTK_LABEL(StatusBar), text ? text : "");
	g_timeout_add((guint)time, pop_message, NULL);
}

gboolean pop_message(gpointer user_data)
{
	gtk_label_set_text(GTK_LABEL(StatusBar), "");
	return FALSE;
}

void clear_display(void)
{
	initialize_hexadecimal_display();
	if (display)
		vte_terminal_reset(VTE_TERMINAL(display), TRUE, TRUE);
}

gboolean on_hex_key_pressed(GtkEventControllerKey *ctrl,
                            guint keyval, guint keycode,
                            GdkModifierType state, gpointer user_data)
{
	GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(ctrl));

	switch (keyval)
	{
	case GDK_KEY_Up:
		set_saved_data(widget, TRUE);
		return TRUE;
	case GDK_KEY_Down:
		set_saved_data(widget, FALSE);
		return TRUE;
	default:
		return FALSE;
	}
}

void update_hex_history(GtkWidget *widget)
{
	const gchar *text = gtk_editable_get_text(GTK_EDITABLE(widget));

	if (g_strcmp0(text, "") == 0)
		return;

	if (current_hex && g_strcmp0((const gchar *)current_hex->data, text) == 0)
		hex_history = g_list_remove(hex_history, current_hex->data);

	hex_history = g_list_append(hex_history, g_strdup(text));
	current_hex = NULL;
}

void set_saved_data(GtkWidget *widget, gboolean direction)
{
	const gchar *text = "";

	if (!hex_history)
		return;

	if (direction)
	{
		if (!current_hex)
			current_hex = g_list_last(hex_history);
		else if (current_hex->prev)
			current_hex = current_hex->prev;
		else
			return;
		text = (const gchar *)current_hex->data;
	}
	else
	{
		if (current_hex && current_hex->next)
		{
			current_hex = current_hex->next;
			text = (const gchar *)current_hex->data;
		}
		else
		{
			current_hex = NULL;
		}
	}

	gtk_editable_set_text(GTK_EDITABLE(widget), text);
}

void interface_cleanup(void)
{
	if (accel_snapshot != NULL)
	{
		g_hash_table_destroy(accel_snapshot);
		accel_snapshot = NULL;
	}

	if (hex_history != NULL)
	{
		g_list_free_full(hex_history, g_free);
		hex_history = NULL;
		current_hex = NULL;
	}
}
