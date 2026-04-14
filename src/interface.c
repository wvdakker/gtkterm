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
#elif defined (HAVE_SYS_TTYCOM_H)
#endif
#include <vte/vte.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "term_config.h"
#include "port_config_dialog.h"
#include "terminal_config.h"
#include "config_dialog.h"
#include "config_file.h"
#include "files.h"
#include "search.h"
#include "serial.h"
#include "interface.h"
#include "buffer.h"
#include "macros.h"
#include "auto_config.h"
#include "logging.h"
#include "device_monitor.h"

#include <glib/gprintf.h>
#include <glib/gi18n.h>

gboolean echo_on;
gboolean autoreconnect_on;
gboolean crlfauto_on;
gboolean esc_clear_screen_on;
gboolean timestamp_on = 0;
static gboolean hotkeys_disabled = FALSE;
GtkWidget *StatusBar;
GtkWidget *signals[6];
static GtkWidget *Hex_Box;
GtkWidget *searchBar;
GtkWidget *scrolled_window;
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

extern struct configuration_port config;

/* Variables for hexadecimal display */
static guint bytes_per_line = 16;
static gchar blank_data[128];
static guint total_bytes;
static gboolean show_index = FALSE;
guint virt_col_pos = 0;

/* Forward declarations */
gboolean pop_message(gpointer user_data);
static gboolean Send_Hexadecimal(GtkWidget *widget, gpointer pointer);
void update_hex_history(GtkWidget *widget);
void set_saved_data(GtkWidget *widget, gboolean direction);
void update_copy_sensivity(VteTerminal *terminal, gpointer data);
void show_control_signals(int stat);
static void Got_Input(VteTerminal *widget, gchar *text, guint length, gpointer ptr);
static gboolean on_hex_key_pressed(GtkEventControllerKey *ctrl,
                                    guint keyval, guint keycode,
                                    GdkModifierType state, gpointer user_data);

static void file_exit_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	save_window_geometry();
	g_application_quit(G_APPLICATION(main_app));
}

static gboolean on_main_window_close_request(GtkWidget *widget, gpointer data)
{
	save_window_geometry();
	return FALSE;
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
	Set_signals(GPOINTER_TO_INT(data));
}

static void signals_close_port_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	interface_close_port();
}

static void signals_open_port_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	interface_open_port();
}

static void help_about_cb(GSimpleAction *a, GVariant *p, gpointer data)
{
	const gchar *authors[] = {"Julien Schimtt", "Zach Davis", "Florian Euchner", "Stephan Enderlein",
	                    "Kevin Picot", NULL};
	gchar *comments_program = _("GTKTerm is a simple GTK+ terminal used to communicate with the serial port.");
	gchar comments[256];

	g_sprintf(comments, "%s\n\n%s", RELEASE_DATE, comments_program);

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
}

/* Toggle / radio change-state callbacks */

static const struct { const char *action; const char *accel; } app_accels[] = {
	{ "app.file-exit",          "<Shift><Control>Q" },
	{ "app.clear-screen",       "<Shift><Control>L" },
	{ "app.edit-copy",          "<Shift><Control>C" },
	{ "app.edit-paste",         "<Shift><Control>V" },
	{ "app.edit-find",          "<Shift><Control>F" },
	{ "app.edit-select-all",    "<Shift><Control>A" },
	{ "app.config-port",        "<Shift><Control>S" },
	{ "app.signals-send-break", "<Shift><Control>B" },
	{ "app.signals-open-port",  "F5" },
	{ "app.signals-close-port", "F6" },
	{ "app.signals-dtr",        "F7" },
	{ "app.signals-rts",        "F8" },
};

static void echo_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	echo_on = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
	configure_echo(echo_on);
}

static void autoreconnect_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	autoreconnect_on = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
	configure_autoreconnect_enable(autoreconnect_on);
}

static void crlfauto_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	crlfauto_on = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
	configure_crlfauto(crlfauto_on);
}

static void esc_clear_screen_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	esc_clear_screen_on = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
	configure_esc_clear_screen(esc_clear_screen_on);
}

static void timestamp_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	timestamp_on = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
	config.timestamp = timestamp_on;
}

static void disable_hotkeys_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	hotkeys_disabled = g_variant_get_boolean(s);
	config.disable_hotkeys = hotkeys_disabled;
	g_simple_action_set_state(a, s);
	set_macros_shortcuts_enabled(!hotkeys_disabled);
	for (gsize i = 0; i < G_N_ELEMENTS(app_accels); i++) {
		if (hotkeys_disabled) {
			const char *empty[] = { NULL };
			gtk_application_set_accels_for_action(main_app, app_accels[i].action, empty);
		} else {
			const char *accel_list[] = { app_accels[i].accel, NULL };
			gtk_application_set_accels_for_action(main_app, app_accels[i].action, accel_list);
		}
	}
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
	if (g_variant_get_boolean(s))
		gtk_widget_set_visible(Hex_Box, TRUE);
	else
		gtk_widget_set_visible(Hex_Box, FALSE);
}

static void view_mode_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	const gchar *mode = g_variant_get_string(s, NULL);
	g_simple_action_set_state(a, s);
	if (g_str_equal(mode, "hex"))
		set_view(HEXADECIMAL_VIEW);
	else
		set_view(ASCII_VIEW);
}

static void hex_chars_change_state(GSimpleAction *a, GVariant *s, gpointer data)
{
	const gchar *val = g_variant_get_string(s, NULL);
	g_simple_action_set_state(a, s);
	bytes_per_line = atoi(val);
	set_view(HEXADECIMAL_VIEW);
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
	{ "help-about",         help_about_cb },
	/* Stateful toggles (NULL activate, type, initial state, change-state) */
	{ "local-echo",         NULL, NULL, "false", echo_change_state },
	{ "autoreconnect",      NULL, NULL, "false", autoreconnect_change_state },
	{ "crlfauto",           NULL, NULL, "false", crlfauto_change_state },
	{ "esc-clear-screen",   NULL, NULL, "false", esc_clear_screen_change_state },
	{ "timestamp",          NULL, NULL, "false", timestamp_change_state },
	{ "disable-hotkeys",    NULL, NULL, "false", disable_hotkeys_change_state },
	{ "view-index",         NULL, NULL, "false", view_index_change_state },
	{ "view-send-hex",      NULL, NULL, "false", view_send_hex_change_state },
	/* Stateful radio actions */
	{ "view-mode",          NULL, "s",  "'ascii'", view_mode_change_state },
	{ "hex-chars",          NULL, "s",  "'16'",    hex_chars_change_state },
	/* Simple action used only for enabling/disabling the hex-chars submenu */
	{ "hex-chars-submenu",  NULL },
};



static void register_keyboard_shortcuts(GtkApplication *app)
{
	for (gsize i = 0; i < G_N_ELEMENTS(app_accels); i++) {
		const char *accel_list[] = { app_accels[i].accel, NULL };
		gtk_application_set_accels_for_action(app, app_accels[i].action, accel_list);
	}
}

/* Exported helpers that update action state */

void Set_local_echo(gboolean echo)
{
	echo_on = echo;
	GAction *action = g_action_map_lookup_action(G_ACTION_MAP(main_app), "local-echo");
	if (action)
		g_simple_action_set_state(G_SIMPLE_ACTION(action),
		                          g_variant_new_boolean(echo_on));
}

void Set_crlfauto(gboolean crlfauto)
{
	crlfauto_on = crlfauto;
	GAction *action = g_action_map_lookup_action(G_ACTION_MAP(main_app), "crlfauto");
	if (action)
		g_simple_action_set_state(G_SIMPLE_ACTION(action),
		                          g_variant_new_boolean(crlfauto_on));
}

void Set_autoreconnect_enabled(gboolean autoreconnect_enabled)
{
	autoreconnect_on = autoreconnect_enabled;
	GAction *action = g_action_map_lookup_action(G_ACTION_MAP(main_app), "autoreconnect");
	if (action)
		g_simple_action_set_state(G_SIMPLE_ACTION(action),
		                          g_variant_new_boolean(autoreconnect_on));
}

void Set_esc_clear_screen(gboolean esc_clear_screen)
{
	esc_clear_screen_on = esc_clear_screen;
	GAction *action = g_action_map_lookup_action(G_ACTION_MAP(main_app), "esc-clear-screen");
	if (action)
		g_simple_action_set_state(G_SIMPLE_ACTION(action),
		                          g_variant_new_boolean(esc_clear_screen_on));
}

void Set_timestamp(gboolean timestamp)
{
	timestamp_on = timestamp;
	GAction *action = g_action_map_lookup_action(G_ACTION_MAP(main_app), "timestamp");
	if (action)
		g_simple_action_set_state(G_SIMPLE_ACTION(action),
		                          g_variant_new_boolean(timestamp_on));
}

void Set_hotkeys_disabled(gboolean disabled)
{
	hotkeys_disabled = disabled;
	set_macros_shortcuts_enabled(!hotkeys_disabled);
	GAction *action = g_action_map_lookup_action(G_ACTION_MAP(main_app), "disable-hotkeys");
	if (action)
		g_simple_action_set_state(G_SIMPLE_ACTION(action),
		                          g_variant_new_boolean(hotkeys_disabled));
	for (gsize i = 0; i < G_N_ELEMENTS(app_accels); i++) {
		if (hotkeys_disabled) {
			const char *empty[] = { NULL };
			gtk_application_set_accels_for_action(main_app, app_accels[i].action, empty);
		} else {
			const char *accel_list[] = { app_accels[i].accel, NULL };
			gtk_application_set_accels_for_action(main_app, app_accels[i].action, accel_list);
		}
	}
}

static gboolean reenable_commit_cb(gpointer data)
{
	if (--pending_reenable == 0 && got_input_handler_id && display)
		g_signal_handler_unblock(display, got_input_handler_id);
	return G_SOURCE_REMOVE;
}

void set_view(guint type)
{
	GAction *show_index_action    = g_action_map_lookup_action(G_ACTION_MAP(main_app), "view-index");
	GAction *hex_chars_action     = g_action_map_lookup_action(G_ACTION_MAP(main_app), "hex-chars");
	GAction *hex_chars_sub_action = g_action_map_lookup_action(G_ACTION_MAP(main_app), "hex-chars-submenu");
	GAction *view_mode_action     = g_action_map_lookup_action(G_ACTION_MAP(main_app), "view-mode");

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
		if (view_mode_action)
			g_simple_action_set_state(G_SIMPLE_ACTION(view_mode_action),
			                          g_variant_new_string("ascii"));
		if (show_index_action)
			g_simple_action_set_enabled(G_SIMPLE_ACTION(show_index_action), FALSE);
		if (hex_chars_action)
			g_simple_action_set_enabled(G_SIMPLE_ACTION(hex_chars_action), FALSE);
		if (hex_chars_sub_action)
			g_simple_action_set_enabled(G_SIMPLE_ACTION(hex_chars_sub_action), FALSE);
		total_bytes = 0;
		set_display_func(put_text);
		break;
	case HEXADECIMAL_VIEW:
		if (view_mode_action)
			g_simple_action_set_state(G_SIMPLE_ACTION(view_mode_action),
			                          g_variant_new_string("hex"));
		if (show_index_action)
			g_simple_action_set_enabled(G_SIMPLE_ACTION(show_index_action), TRUE);
		if (hex_chars_action)
			g_simple_action_set_enabled(G_SIMPLE_ACTION(hex_chars_action), TRUE);
		if (hex_chars_sub_action)
			g_simple_action_set_enabled(G_SIMPLE_ACTION(hex_chars_sub_action), TRUE);
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
	GSimpleAction *action;

	action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(main_app), "log-to-file"));
	if (action) g_simple_action_set_enabled(action, !currentlyLogging);
	action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(main_app), "log-pause-resume"));
	if (action) g_simple_action_set_enabled(action, currentlyLogging);
	action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(main_app), "log-stop"));
	if (action) g_simple_action_set_enabled(action, currentlyLogging);
	action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(main_app), "log-clear"));
	if (action) g_simple_action_set_enabled(action, currentlyLogging);
}

/* Right-click popup */

static void on_right_click_pressed(GtkGestureClick *gesture, int n_press,
                                   double x, double y, gpointer data)
{
	GtkPopoverMenu *pmenu = GTK_POPOVER_MENU(data);
	GdkRectangle rect = { (int)x, (int)y, 1, 1 };
	gtk_popover_set_pointing_to(GTK_POPOVER(pmenu), &rect);
	gtk_popover_popup(GTK_POPOVER(pmenu));
}

static gboolean terminal_popup_key_cb(GtkEventControllerKey *ctrl,
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

/* ---- Main window creation ---- */

void create_main_window(GtkApplication *app)
{
	GtkWidget *hex_send_entry;

	main_app = app;

	/* Register all actions on the application */
	g_action_map_add_action_entries(G_ACTION_MAP(app), app_actions,
	                                G_N_ELEMENTS(app_actions), NULL);

	register_keyboard_shortcuts(app);

	/* Load window layout and menu models from resource */
	GtkBuilderCScope *scope = GTK_BUILDER_CSCOPE(gtk_builder_cscope_new());
	gtk_builder_cscope_add_callback_symbols(scope,
	    "g_application_quit", G_CALLBACK(g_application_quit),
	    "Send_Hexadecimal",   G_CALLBACK(Send_Hexadecimal),
	    NULL);
	GtkBuilder *builder = gtk_builder_new();
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
	scrolled_window = GTK_WIDGET(gtk_builder_get_object(builder, "scrolled_window"));
	Hex_Box         = GTK_WIDGET(gtk_builder_get_object(builder, "hex_box"));
	hex_send_entry  = GTK_WIDGET(gtk_builder_get_object(builder, "hex_send_entry"));
	StatusBar       = GTK_WIDGET(gtk_builder_get_object(builder, "status_bar"));
	signals[5]      = GTK_WIDGET(gtk_builder_get_object(builder, "signal_dtr"));
	signals[4]      = GTK_WIDGET(gtk_builder_get_object(builder, "signal_rts"));
	signals[3]      = GTK_WIDGET(gtk_builder_get_object(builder, "signal_cts"));
	signals[2]      = GTK_WIDGET(gtk_builder_get_object(builder, "signal_cd"));
	signals[1]      = GTK_WIDGET(gtk_builder_get_object(builder, "signal_dsr"));
	signals[0]      = GTK_WIDGET(gtk_builder_get_object(builder, "signal_ri"));

	/* Click-to-toggle for output signals DTR and RTS */
	GtkGesture *dtr_click = gtk_gesture_click_new();
	g_signal_connect(dtr_click, "pressed",
	                 G_CALLBACK(on_signal_label_clicked), GINT_TO_POINTER(0));
	gtk_widget_add_controller(signals[5], GTK_EVENT_CONTROLLER(dtr_click));

	GtkGesture *rts_click = gtk_gesture_click_new();
	g_signal_connect(rts_click, "pressed",
	                 G_CALLBACK(on_signal_label_clicked), GINT_TO_POINTER(1));
	gtk_widget_add_controller(signals[4], GTK_EVENT_CONTROLLER(rts_click));

	/* VTE terminal */
	display = vte_terminal_new();

	vte_terminal_set_scroll_on_output(VTE_TERMINAL(display), FALSE);
	vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(display), TRUE);
	vte_terminal_set_mouse_autohide(VTE_TERMINAL(display), TRUE);
	vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(display), VTE_CURSOR_BLINK_OFF);
	vte_terminal_set_backspace_binding(VTE_TERMINAL(display),
	                                   VTE_ERASE_ASCII_BACKSPACE);

	clear_display();

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window),
	                              GTK_WIDGET(display));

	/* Search bar — inserted into its placeholder box */
	GtkWidget *sb_placeholder = GTK_WIDGET(gtk_builder_get_object(builder, "search_bar_placeholder"));
	searchBar = search_bar_new(GTK_WINDOW(Fenetre), VTE_TERMINAL(display));
	gtk_box_append(GTK_BOX(sb_placeholder), GTK_WIDGET(searchBar));

	/* Right-click popup — constructed from model declared in the builder */
	popup_menu = GTK_WIDGET(gtk_builder_get_object(builder, "popup_menu"));
	gtk_widget_set_parent(popup_menu, GTK_WIDGET(display));
	/* Unparent before window teardown to avoid assertion in g_signal_handlers_destroy */
	g_signal_connect_swapped(Fenetre, "close-request",
	                         G_CALLBACK(gtk_widget_unparent), popup_menu);

	g_object_unref(builder);

	GtkGesture *right_click = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(right_click), GDK_BUTTON_SECONDARY);
	g_signal_connect(right_click, "pressed",
	                 G_CALLBACK(on_right_click_pressed), popup_menu);
	gtk_widget_add_controller(GTK_WIDGET(display), GTK_EVENT_CONTROLLER(right_click));

	/* Keyboard-triggered popup (Menu key) */
	GtkEventController *popup_key_ctrl = gtk_event_controller_key_new();
	g_signal_connect(popup_key_ctrl, "key-pressed",
	                 G_CALLBACK(terminal_popup_key_cb), popup_menu);
	gtk_widget_add_controller(GTK_WIDGET(display), popup_key_ctrl);

	g_signal_connect(G_OBJECT(display), "selection-changed",
	                 G_CALLBACK(update_copy_sensivity), NULL);
	update_copy_sensivity(VTE_TERMINAL(display), NULL);

	toggle_logging_pause_resume(FALSE);
	toggle_logging_sensitivity(FALSE);

	/* Hex send entry signals */
	GtkEventController *key_ctrl = gtk_event_controller_key_new();
	g_signal_connect(key_ctrl, "key-pressed",
	                 G_CALLBACK(on_hex_key_pressed), NULL);
	gtk_widget_add_controller(hex_send_entry, key_ctrl);

	got_input_handler_id = g_signal_connect_after(GTK_WIDGET(display), "commit",
	                                              G_CALLBACK(Got_Input), NULL);

	/* Install GtkShortcutController for macro shortcuts on the main window */
	install_macro_shortcut_controller(Fenetre);

	g_signal_connect(Fenetre, "close-request",
	                 G_CALLBACK(on_main_window_close_request), NULL);

	load_window_geometry();

	gtk_widget_set_visible(Fenetre, TRUE);
	search_bar_hide(searchBar);
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
		if (echo_on)
			put_chars(string, (size_t)bytes_written, crlfauto_on);
	}
	return (gint)bytes_written;
}

static void Got_Input(VteTerminal *widget, gchar *text, guint length, gpointer ptr)
{
	if (esc_clear_screen_on && length >= 1 && (guchar)text[0] == 0x1b)
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
	GSimpleAction *action = G_SIMPLE_ACTION(
		g_action_map_lookup_action(G_ACTION_MAP(main_app), "edit-copy"));
	if (action)
		g_simple_action_set_enabled(action, can_copy);
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

gboolean control_signals_read(void)
{
	int state;

	state = lis_sig();
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
	Config_port();
	gchar *message = get_port_string();
	Set_status_message(message);
	Set_window_title(message);
	g_free(message);
}

void interface_close_port(void)
{
	Close_port();
	gchar *message = get_port_string();
	Set_status_message(message);
	Set_window_title(message);
	g_free(message);
}

void show_message(const gchar *message, gint type_msg)
{
	if (type_msg != MSG_ERR && type_msg != MSG_WRN)
		return;

	GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", message);
	gtk_alert_dialog_set_modal(dialog, TRUE);
	gtk_alert_dialog_show(dialog, GTK_WINDOW(Fenetre));
	g_object_unref(dialog);
}

static gboolean Send_Hexadecimal(GtkWidget *widget, gpointer pointer)
{
	guint i;
	gchar *message, **tokens, *buff;
	guint scan_val;

	const gchar *text = gtk_editable_get_text(GTK_EDITABLE(widget));

	if (strlen(text) == 0)
	{
		Put_temp_message(_("0 byte(s) sent!"), 1500);
		gtk_editable_set_text(GTK_EDITABLE(widget), "");
		return FALSE;
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
			return FALSE;
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

	return FALSE;
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

static gboolean on_hex_key_pressed(GtkEventControllerKey *ctrl,
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
	if (!hex_history)
		return;

	const gchar *text = "";

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
