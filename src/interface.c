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
#include "logging.h"
#include "device_monitor.h"

#include <glib/gi18n.h>
#include "config_file.h"

GtkWidget *StatusBar;
GtkWidget *signals[6];
static GtkWidget *Hex_Box;
GtkWidget *searchBar;
GtkWindow *Fenetre;
static GtkApplication *main_app;
VteTerminal *display = NULL;
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
static void set_action_enabled(const char *name, gboolean enabled);
void update_hex_history(GtkWidget *widget);
void set_saved_data(GtkWidget *widget, gboolean direction);
void show_control_signals(int stat);
void update_copy_sensivity(VteTerminal *terminal, gpointer data);
static void Got_Input(VteTerminal *widget, gchar *text, guint length, gpointer ptr);
gboolean control_signals_read(gpointer user_data);
gboolean on_hex_key_pressed(GtkEventControllerKey *ctrl,
                                    guint keyval, guint keycode,
                                    GdkModifierType state, gpointer user_data);

static void file_exit_cb(GSimpleAction *a G_GNUC_UNUSED, GVariant *p G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	g_application_quit(G_APPLICATION(main_app));
}

static void clear_screen_cb(GSimpleAction *a G_GNUC_UNUSED, GVariant *p G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	clear_buffer();
}

static void clear_scrollback_cb(GSimpleAction *a G_GNUC_UNUSED, GVariant *p G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	clear_scrollback();
}

static void edit_copy_cb(GSimpleAction *a G_GNUC_UNUSED, GVariant *p G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	vte_terminal_copy_clipboard_format(display, VTE_FORMAT_TEXT);
}

static void edit_paste_cb(GSimpleAction *a G_GNUC_UNUSED, GVariant *p G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	vte_terminal_paste_clipboard(display);
}

static void edit_find_cb(GSimpleAction *a G_GNUC_UNUSED, GVariant *p G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	if (gtk_widget_is_visible(searchBar))
		search_bar_hide(searchBar);
	else
		search_bar_show(searchBar);
}

static void edit_select_all_cb(GSimpleAction *a G_GNUC_UNUSED, GVariant *p G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	vte_terminal_select_all(display);
}

static void signals_send_break_cb(GSimpleAction *a G_GNUC_UNUSED, GVariant *p G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	sendbreak();
	Put_temp_message(_("Break signal sent!"), 800);
}

static void signals_toggle_DTR_cb(GSimpleAction *a G_GNUC_UNUSED, GVariant *p G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	Set_signals(0);
}

static void signals_toggle_RTS_cb(GSimpleAction *a G_GNUC_UNUSED, GVariant *p G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	Set_signals(1);
}

static void on_dtr_clicked(GtkGestureClick *gesture G_GNUC_UNUSED, int n_press G_GNUC_UNUSED,
                           double x G_GNUC_UNUSED, double y G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	Set_signals(0);
}

static void on_rts_clicked(GtkGestureClick *gesture G_GNUC_UNUSED, int n_press G_GNUC_UNUSED,
                           double x G_GNUC_UNUSED, double y G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	Set_signals(1);
}

static void signals_close_port_cb(GSimpleAction *a G_GNUC_UNUSED, GVariant *p G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	interface_close_port();
}

static void signals_open_port_cb(GSimpleAction *a G_GNUC_UNUSED, GVariant *p G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	interface_open_port();
}

static void help_shortcuts_cb(GSimpleAction *a G_GNUC_UNUSED, GVariant *p G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	GtkBuilder *builder = gtk_builder_new_from_resource("/org/gtk/gtkterm/shortcuts_window.ui");
	GtkWindow *win = GTK_WINDOW(gtk_builder_get_object(builder, "shortcuts_window"));

	gtk_window_set_transient_for(win, Fenetre);
	gtk_window_present(win);
	g_object_unref(builder);
}

static void help_about_cb(GSimpleAction *a G_GNUC_UNUSED, GVariant *p G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	GtkBuilder *builder = gtk_builder_new_from_resource("/org/gtk/gtkterm/about_dialog.ui");
	GtkAboutDialog *dlg = GTK_ABOUT_DIALOG(gtk_builder_get_object(builder, "about_dialog"));
	g_object_unref(builder);

	gtk_window_set_transient_for(GTK_WINDOW(dlg), Fenetre);
	gtk_window_present(GTK_WINDOW(dlg));
}

/* Toggle / radio change-state callbacks */

static void echo_change_state(GSimpleAction *a, GVariant *s, gpointer data G_GNUC_UNUSED)
{
	term_conf.echo = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
}

static void autoreconnect_change_state(GSimpleAction *a, GVariant *s, gpointer data G_GNUC_UNUSED)
{
	term_conf.autoreconnect_enabled = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
}

static void crlfauto_change_state(GSimpleAction *a, GVariant *s, gpointer data G_GNUC_UNUSED)
{
	term_conf.crlfauto = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
}

static void esc_clear_screen_change_state(GSimpleAction *a, GVariant *s, gpointer data G_GNUC_UNUSED)
{
	term_conf.esc_clear_screen = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
}

static void timestamp_change_state(GSimpleAction *a, GVariant *s, gpointer data G_GNUC_UNUSED)
{
	term_conf.timestamp = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
}

static void view_index_change_state(GSimpleAction *a, GVariant *s, gpointer data G_GNUC_UNUSED)
{
	show_index = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
	set_view(HEXADECIMAL_VIEW);
}

static void view_send_hex_change_state(GSimpleAction *a, GVariant *s, gpointer data G_GNUC_UNUSED)
{
	g_simple_action_set_state(a, s);
	gtk_widget_set_visible(Hex_Box, g_variant_get_boolean(s));
}

static void view_mode_change_state(GSimpleAction *a, GVariant *s, gpointer data G_GNUC_UNUSED)
{
	const gchar *mode = g_variant_get_string(s, NULL);
	g_simple_action_set_state(a, s);
	set_view(g_str_equal(mode, "hex") ? HEXADECIMAL_VIEW : ASCII_VIEW);
}

static void hex_chars_change_state(GSimpleAction *a, GVariant *s, gpointer data G_GNUC_UNUSED)
{
	const gchar *val = g_variant_get_string(s, NULL);
	g_simple_action_set_state(a, s);
	bytes_per_line = atoi(val);
	set_view(HEXADECIMAL_VIEW);
}

/* All application accelerators in one place.  Used both at startup to
 * register them and in apply_shortcuts_disabled() to toggle them. */
static const struct { const gchar *action; const gchar *accel; } app_accels[] = {
	{ "app.clear-screen",       "<Shift><Control>L" },
	{ "app.file-exit",          "<Shift><Control>Q" },
	{ "app.edit-copy",          "<Shift><Control>C" },
	{ "app.edit-paste",         "<Shift><Control>V" },
	{ "app.edit-find",          "<Shift><Control>F" },
	{ "app.edit-select-all",    "<Shift><Control>A" },
	{ "app.config-port",        "<Shift><Control>S" },
	{ "app.signals-send-break", "<Shift><Control>B" },
	{ "app.signals-open-port",  "F5"                },
	{ "app.signals-close-port", "F6"                },
	{ "app.signals-dtr",        "F7"                },
	{ "app.signals-rts",        "F8"                },
	{ "app.help-shortcuts",     "<Control>question" },
};

static void apply_shortcuts_disabled(gboolean disabled)
{
	guint i;

	set_macros_shortcuts_enabled(!disabled);
	set_action_enabled("macros", !disabled);
	set_action_enabled("help-shortcuts", !disabled);

	for (i = 0; i < G_N_ELEMENTS(app_accels); i++) {
		if (disabled) {
			const char *empty[] = { NULL };
			gtk_application_set_accels_for_action(main_app, app_accels[i].action, empty);
		} else {
			const char *av[] = { app_accels[i].accel, NULL };
			gtk_application_set_accels_for_action(main_app, app_accels[i].action, av);
		}
	}
}

static void disable_shortcuts_change_state(GSimpleAction *a, GVariant *s, gpointer data G_GNUC_UNUSED)
{
	term_conf.disable_shortcuts = g_variant_get_boolean(s);
	g_simple_action_set_state(a, s);
	apply_shortcuts_disabled(term_conf.disable_shortcuts);
}

/* Normal action entries table */
static const GActionEntry app_actions[] = {
	{ .name = "file-exit",          .activate = file_exit_cb          },
	{ .name = "clear-screen",       .activate = clear_screen_cb       },
	{ .name = "clear-scrollback",   .activate = clear_scrollback_cb   },
	{ .name = "send-raw-file",      .activate = send_raw_file         },
	{ .name = "save-raw-file",      .activate = save_raw_file         },
	{ .name = "save-ascii-file",    .activate = save_ascii_file       },
	{ .name = "edit-copy",          .activate = edit_copy_cb          },
	{ .name = "edit-paste",         .activate = edit_paste_cb         },
	{ .name = "edit-find",          .activate = edit_find_cb          },
	{ .name = "edit-select-all",    .activate = edit_select_all_cb    },
	{ .name = "log-to-file",        .activate = logging_start         },
	{ .name = "log-pause-resume",   .activate = logging_pause_resume  },
	{ .name = "log-stop",           .activate = logging_stop          },
	{ .name = "log-clear",          .activate = logging_clear         },
	{ .name = "config-port",        .activate = Config_Port_Fenetre   },
	{ .name = "config-terminal",    .activate = Config_Terminal       },
	{ .name = "macros",             .activate = Config_macros         },
	{ .name = "select-config",      .activate = select_config_callback },
	{ .name = "save-config",        .activate = save_config_callback  },
	{ .name = "delete-config",      .activate = delete_config_callback },
	{ .name = "signals-send-break", .activate = signals_send_break_cb },
	{ .name = "signals-open-port",  .activate = signals_open_port_cb  },
	{ .name = "signals-close-port", .activate = signals_close_port_cb },
	{ .name = "signals-dtr",        .activate = signals_toggle_DTR_cb },
	{ .name = "signals-rts",        .activate = signals_toggle_RTS_cb },
	{ .name = "help-shortcuts",     .activate = help_shortcuts_cb     },
	{ .name = "help-about",         .activate = help_about_cb         },
	/* Stateful toggles */
	{ .name = "local-echo",       .state = "false", .change_state = echo_change_state           },
	{ .name = "autoreconnect",    .state = "false", .change_state = autoreconnect_change_state  },
	{ .name = "crlfauto",         .state = "false", .change_state = crlfauto_change_state       },
	{ .name = "esc-clear-screen", .state = "false", .change_state = esc_clear_screen_change_state },
	{ .name = "timestamp",        .state = "false", .change_state = timestamp_change_state      },
	{ .name = "view-index",       .state = "false", .change_state = view_index_change_state     },
	{ .name = "view-send-hex",    .state = "false", .change_state = view_send_hex_change_state  },
	/* Stateful radio actions */
	{ .name = "view-mode", .parameter_type = "s", .state = "'ascii'", .change_state = view_mode_change_state },
	{ .name = "hex-chars", .parameter_type = "s", .state = "'16'",    .change_state = hex_chars_change_state },
	/* Simple action used only for enabling/disabling the hex-chars submenu */
	{ .name = "hex-chars-submenu" },
	{ .name = "disable-shortcuts", .state = "false", .change_state = disable_shortcuts_change_state },
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

void Set_local_echo(gboolean v)            { term_conf.echo                  = v; set_bool_action("local-echo",       v); }
void Set_crlfauto(gboolean v)              { term_conf.crlfauto              = v; set_bool_action("crlfauto",         v); }
void Set_autoreconnect_enabled(gboolean v) { term_conf.autoreconnect_enabled = v; set_bool_action("autoreconnect",    v); }
void Set_esc_clear_screen(gboolean v)      { term_conf.esc_clear_screen      = v; set_bool_action("esc-clear-screen", v); }
void Set_timestamp(gboolean v)             { term_conf.timestamp             = v; set_bool_action("timestamp",        v); }

void Set_shortcuts_disabled(gboolean disabled)
{
	term_conf.disable_shortcuts = disabled;
	set_bool_action("disable-shortcuts", disabled);
	apply_shortcuts_disabled(disabled);
}

static gboolean reenable_commit_cb(gpointer data G_GNUC_UNUSED)
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

void toggle_logging_pause_resume(gboolean currentlyLogging G_GNUC_UNUSED)
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

gboolean terminal_popup_key_cb(GtkEventControllerKey *ctrl G_GNUC_UNUSED,
                                      guint keyval, guint keycode G_GNUC_UNUSED,
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
	PangoFontDescription *desc;

	if (!display)
		return;

	vte_terminal_set_size(display, term_conf.rows, term_conf.columns);
	vte_terminal_set_scrollback_lines(display, term_conf.scrollback);
	vte_terminal_set_color_foreground(display, &term_conf.foreground_color);
	vte_terminal_set_color_background(display, &term_conf.background_color);
	vte_terminal_set_cursor_shape(display,
	    term_conf.block_cursor ? VTE_CURSOR_SHAPE_BLOCK : VTE_CURSOR_SHAPE_IBEAM);

	desc = pango_font_description_from_string(term_conf.font);
	vte_terminal_set_font(display, desc);
	pango_font_description_free(desc);

	gtk_widget_queue_draw(GTK_WIDGET(display));
}

/* ---- Main window creation ---- */

void create_main_window(GtkApplication *app)
{
	GtkBuilderCScope *scope;
	GtkBuilder *builder;
	GtkWidget *sb_placeholder;
	guint i;

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
	    "terminal_popup_key_cb",      G_CALLBACK(terminal_popup_key_cb),
	    "update_copy_sensivity",      G_CALLBACK(update_copy_sensivity),
	    "gtk_widget_unparent",        G_CALLBACK(gtk_widget_unparent),
	    NULL);

	builder = gtk_builder_new();
	gtk_builder_set_scope(builder, GTK_BUILDER_SCOPE(scope));
	g_object_unref(scope);
	gtk_builder_add_from_resource(builder, "/org/gtk/gtkterm/main_window.ui", NULL);

	Fenetre = GTK_WINDOW(gtk_builder_get_object(builder, "main_window"));
	gtk_window_set_application(Fenetre, GTK_APPLICATION(app));

	/* Register all keyboard accelerators from the shared table. */
	for (i = 0; i < G_N_ELEMENTS(app_accels); i++) {
		const gchar *av[] = { app_accels[i].accel, NULL };
		gtk_application_set_accels_for_action(app, app_accels[i].action, av);
	}

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
	display = VTE_TERMINAL(gtk_builder_get_object(builder, "display"));

	clear_display();

	/* Search bar — inserted into its placeholder box */
	sb_placeholder = GTK_WIDGET(gtk_builder_get_object(builder, "search_bar_placeholder"));
	searchBar = search_bar_new(Fenetre, display);
	gtk_box_append(GTK_BOX(sb_placeholder), searchBar);

	g_object_unref(builder);

	set_action_enabled("edit-copy", FALSE);

	toggle_logging_sensitivity(FALSE);

	got_input_handler_id = g_signal_connect_after(display, "commit",
	                                              G_CALLBACK(Got_Input), NULL);

	g_timeout_add(POLL_DELAY, control_signals_read, NULL);

	install_macro_shortcut_controller(GTK_WIDGET(display));

	if (term_conf.window_width > 0 && term_conf.window_height > 0)
	{
		GdkDisplay *gdk_disp = gdk_display_get_default();
		int w = term_conf.window_width;
		int h = term_conf.window_height;

		if (gdk_disp)
		{
			GListModel *monitors = gdk_display_get_monitors(gdk_disp);
			if (monitors && g_list_model_get_n_items(monitors) > 0)
			{
				GdkMonitor *monitor = g_list_model_get_item(monitors, 0);
				if (monitor)
				{
					GdkRectangle workarea;
					gdk_monitor_get_geometry(monitor, &workarea);
					w = CLAMP(w, 100, workarea.width);
					h = CLAMP(h, 100, workarea.height);
					g_object_unref(monitor);
				}
			}
		}
		gtk_window_set_default_size(Fenetre, w, h);
	}

	gtk_widget_set_visible(GTK_WIDGET(Fenetre), TRUE);
}

void initialize_hexadecimal_display(void)
{
	total_bytes = 0;
	memset(blank_data, ' ', 128);
	blank_data[bytes_per_line * 3 + 5] = 0;
}

void put_hexadecimal(const gchar *string, gsize size)
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
					vte_terminal_feed(display, data, strlen(data));
				}
			}

			sprintf(data_byte, "%02X ", (guchar)string[i]);
			log_chars(data_byte, 3);
			vte_terminal_feed(display, data_byte, 3);

			avance = (bytes_per_line - virt_col_pos) * 3 + virt_col_pos + 2;
			sprintf(data_byte, "%c[%uC", 27, avance);
			vte_terminal_feed(display, data_byte, strlen(data_byte));

			ascii[0] = (string[i] > 0x1F) ? string[i] : '.';
			vte_terminal_feed(display, ascii, 1);

			sprintf(data_byte, "%c[%uD", 27, avance + 1U);
			vte_terminal_feed(display, data_byte, strlen(data_byte));

			if (virt_col_pos == bytes_per_line / 2 - 1)
				vte_terminal_feed(display, "- ", strlen("- "));

			virt_col_pos++;
			i++;

			if (virt_col_pos == bytes_per_line)
			{
				vte_terminal_feed(display, "\r\n", 2);
				total_bytes += virt_col_pos;
				virt_col_pos = 0;
			}
		}
	}
}

void put_text(const gchar *string, gsize size)
{
	log_chars(string, (guint)size);
	vte_terminal_feed(display, string, (gssize)size);
}

gssize send_serial(const gchar *string, gsize len)
{
	gssize bytes_written = Send_chars(string, len);
	if (bytes_written > 0)
	{
		if (term_conf.echo)
			put_chars(string, (gsize)bytes_written, term_conf.crlfauto);
	}
	return bytes_written;
}

static void Got_Input(VteTerminal *widget G_GNUC_UNUSED, gchar *text,
	guint length, gpointer data G_GNUC_UNUSED)
{
	if (term_conf.esc_clear_screen && length >= 1 && text[0] == 0x1b)
	{
		clear_buffer();
		clear_display();
	}
	else
		send_serial(text, length);
}

void update_copy_sensivity(VteTerminal *terminal, gpointer data G_GNUC_UNUSED)
{
	gboolean can_copy = vte_terminal_get_has_selection(terminal);
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

gboolean control_signals_read(gpointer user_data G_GNUC_UNUSED)
{
	int state = lis_sig();
	if (state >= 0)
		show_control_signals(state);
	return TRUE;
}

void update_port_status(void)
{
	gchar *message = get_port_string();
	gtk_label_set_text(GTK_LABEL(StatusBar), message ? message : "");
	Set_window_title(message);
	g_free(message);
}

void Set_window_title(const gchar *msg)
{
	gchar *header = g_strdup_printf("GTKTerm - %s", msg);
	gtk_window_set_title(Fenetre, header);
	g_free(header);
}

void interface_open_port(void)
{
	Config_port();
	update_port_status();
}

void interface_close_port(void)
{
	Close_port();
	update_port_status();
}

void show_message(gint type_msg, const gchar *message)
{
	GtkAlertDialog *dialog;

	if (type_msg != MSG_ERR && type_msg != MSG_WRN)
		return;

	dialog = gtk_alert_dialog_new("%s", message);
	gtk_alert_dialog_set_modal(dialog, TRUE);
	gtk_alert_dialog_show(dialog, Fenetre);
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

static void Send_Hexadecimal(GtkWidget *widget, gpointer data G_GNUC_UNUSED)
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

void Put_temp_message(const gchar *text, guint time)
{
	gtk_label_set_text(GTK_LABEL(StatusBar), text ? text : "");
	g_timeout_add(time, pop_message, NULL);
}

gboolean pop_message(gpointer user_data G_GNUC_UNUSED)
{
	gtk_label_set_text(GTK_LABEL(StatusBar), "");
	return FALSE;
}

void clear_display(void)
{
	initialize_hexadecimal_display();
	if (display)
		vte_terminal_reset(display, TRUE, TRUE);
}

gboolean on_hex_key_pressed(GtkEventControllerKey *ctrl,
                            guint keyval, guint keycode G_GNUC_UNUSED,
                            GdkModifierType state G_GNUC_UNUSED,
							gpointer user_data G_GNUC_UNUSED)
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

	if (*text == '\0')
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
	if (hex_history != NULL)
	{
		g_list_free_full(hex_history, g_free);
		hex_history = NULL;
		current_hex = NULL;
	}
}
