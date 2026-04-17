/***********************************************************************/
/* config_file.c                                                       */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      GKeyFile-backed configuration file handling:                   */
/*        - locate / migrate .gtktermrc on first run                   */
/*        - lazy-load and save the GKeyFile singleton                  */
/*        - load / save sections to/from config & term_conf            */
/*        - hard-coded defaults                                        */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <string.h>
#include <glib/gi18n.h>

#include "term_config.h"
#include "config_file.h"
#include "interface.h"
#include "macros.h"
#include "terminal_config.h"
#include "serial.h"

#define CONFIGURATION_FILENAME ".gtktermrc"

/* Public globals — forward-declared as extern in config_file.h */
GFile                    *config_file;
struct configuration_port config;
display_config_t          term_conf;

extern GtkWidget *display;

/* ------------------------------------------------------------------ */
/* GKeyFile singleton                                                  */
/* ------------------------------------------------------------------ */

static GKeyFile *gkf = NULL;

/* Load (lazily) the GKeyFile for the config file.
 * If the file does not exist an empty GKeyFile is returned — the caller
 * creates the content and saves it. */
GKeyFile *get_key_file(void)
{
	if (!gkf)
	{
		GError *err  = NULL;
		gchar  *path;
		gkf  = g_key_file_new();
		path = g_file_get_path(config_file);
		g_key_file_load_from_file(gkf, path, G_KEY_FILE_KEEP_COMMENTS, &err);
		g_free(path);
		g_clear_error(&err); /* ignore "file not found" */
	}
	return gkf;
}

gboolean save_key_file(void)
{
	GError *err = NULL;
	gchar *path = g_file_get_path(config_file);
	gboolean ok = g_key_file_save_to_file(gkf, path, &err);
	g_free(path);
	if (!ok)
	{
		show_message(_("Cannot save configuration file!\n"), MSG_ERR);
		g_clear_error(&err);
	}
	return ok;
}

/* Read a color component stored either as a legacy double (0.0–1.0)
 * or the current integer (0–255).  Returns a value in [0.0, 1.0]. */
static gfloat kf_get_color(GKeyFile *kf, const gchar *section,
                            const gchar *key, gfloat default_val)
{
	gfloat val;
	gchar *s = g_key_file_get_string(kf, section, key, NULL);
	if (!s)
		return default_val;
	if (strchr(s, '.')) {
		/* explicit decimal point → legacy double 0.0–1.0 */
		val = (gfloat)g_ascii_strtod(s, NULL);
	} else {
		gint64 i = g_ascii_strtoll(s, NULL, 10);
		/* g_key_file_set_double writes whole numbers without a decimal
		 * (e.g. 0.0 → "0", 1.0 → "1"), so treat values ≤ 1 as legacy doubles */
		if (i <= 1)
			val = (gfloat)i;           /* legacy whole-number double: 0 or 1 */
		else
			val = (gfloat)i / 255.0f;  /* new integer 0–255 format */
	}
	g_free(s);
	return CLAMP(val, 0.0f, 1.0f);
}

/* GKeyFile-compatible boolean: accepts True/False/Yes/No/1/0 (case-insensitive) */
static gboolean kf_get_bool(GKeyFile *kf, const gchar *section,
                            const gchar *key, gboolean default_val)
{
	gboolean val;
	gchar *s = g_key_file_get_string(kf, section, key, NULL);
	if (!s)
		return default_val;
	val = default_val;
	if (!g_ascii_strcasecmp(s, "true")  || !g_ascii_strcasecmp(s, "yes") || !strcmp(s, "1"))
		val = TRUE;
	else if (!g_ascii_strcasecmp(s, "false") || !g_ascii_strcasecmp(s, "no") || !strcmp(s, "0"))
		val = FALSE;
	g_free(s);
	return val;
}

static void Selec_couleur(GdkRGBA *color, gfloat R, gfloat G, gfloat B, gfloat A)
{
	color->red   = R;
	color->green = G;
	color->blue  = B;
	color->alpha = A;
}

/* ------------------------------------------------------------------ */
/* Initialisation                                                      */
/* ------------------------------------------------------------------ */

void config_file_init(void)
{
	/*
	 * Old location of configuration file was $HOME/.gtktermrc
	 * New location is $XDG_CONFIG_HOME/.gtktermrc
	 *
	 * If configuration file exists at new location, use that one.
	 * Otherwise, if file exists at old location, move file to new location.
	 */
	GFile *config_file_old = g_file_new_build_filename(getenv("HOME"), CONFIGURATION_FILENAME, NULL);
	config_file = g_file_new_build_filename(g_get_user_config_dir(), CONFIGURATION_FILENAME, NULL);

	if (!g_file_query_exists(config_file, NULL) && g_file_query_exists(config_file_old, NULL))
		g_file_move(config_file_old, config_file, G_FILE_COPY_NONE, NULL, NULL, NULL, NULL);
	g_object_unref(config_file_old);
}

void config_file_free(void)
{
	if (config_file != NULL) {
		g_object_unref(config_file);
		config_file = NULL;
	}
	if (gkf != NULL) {
		g_key_file_free(gkf);
		gkf = NULL;
	}
	pango_font_description_free(term_conf.font_desc);
	term_conf.font_desc = NULL;
}

/* ------------------------------------------------------------------ */
/* Defaults                                                            */
/* ------------------------------------------------------------------ */

void Hard_default_configuration(void)
{
	strcpy(config.port, DEFAULT_PORT);
	config.vitesse = DEFAULT_SPEED;
	config.parite = DEFAULT_PARITY;
	config.bits = DEFAULT_BITS;
	config.stops = DEFAULT_STOP;
	config.flux = DEFAULT_FLOW;
	config.delai = DEFAULT_DELAY;
	config.rs485_rts_time_before_transmit = DEFAULT_DELAY_RS485;
	config.rs485_rts_time_after_transmit = DEFAULT_DELAY_RS485;
	config.car = DEFAULT_CHAR;
	config.echo = DEFAULT_ECHO;
	config.crlfauto = FALSE;
	config.autoreconnect_enabled = FALSE;
	config.esc_clear_screen = FALSE;
	config.timestamp = FALSE;
	config.disable_port_lock = FALSE;
	config.disable_hotkeys = FALSE;

	set_terminal_font_from_string(DEFAULT_FONT);

	term_conf.block_cursor = TRUE;
	term_conf.rows = 80;
	term_conf.columns = 25;
	term_conf.scrollback = DEFAULT_SCROLLBACK;
	term_conf.visual_bell = TRUE;

	Selec_couleur(&term_conf.foreground_color, 0.66f, 0.66f, 0.66f, 1.0f);
	Selec_couleur(&term_conf.background_color, 0, 0, 0, 1.0);
}

/* ------------------------------------------------------------------ */
/* Copy current config/term_conf into a GKeyFile section              */
/* ------------------------------------------------------------------ */

void Copy_configuration(GKeyFile *kf, const gchar *section)
{
	static const char *parity_names[] = { "none", "odd",  "even" };
	static const char *flow_names[]   = { "none", "xon",  "rts", "rs485" };
	/* Use a fixed decimal format so the value always contains a '.'
	 * (g_key_file_set_double omits the decimal for whole numbers like
	 * 0.0 → "0", which breaks backward-compatible parsing). */
	static const struct { const gchar *key; const gfloat *val; } color_fields[] = {
		{ "term_foreground_red",   &term_conf.foreground_color.red   },
		{ "term_foreground_green", &term_conf.foreground_color.green },
		{ "term_foreground_blue",  &term_conf.foreground_color.blue  },
		{ "term_foreground_alpha", &term_conf.foreground_color.alpha },
		{ "term_background_red",   &term_conf.background_color.red   },
		{ "term_background_green", &term_conf.background_color.green },
		{ "term_background_blue",  &term_conf.background_color.blue  },
		{ "term_background_alpha", &term_conf.background_color.alpha },
	};
	gchar   *font_raw, *font_quoted;
	macro_t *macros;
	gsize    size;

	g_key_file_set_string( kf, section, "port",                     config.port);
	g_key_file_set_integer(kf, section, "speed",                    (gint)config.vitesse);
	g_key_file_set_integer(kf, section, "bits",                     config.bits);
	g_key_file_set_integer(kf, section, "stopbits",                 config.stops);
	g_key_file_set_string( kf, section, "parity",                   parity_names[CLAMP(config.parite, 0, 2)]);
	g_key_file_set_string( kf, section, "flow",                     flow_names[CLAMP(config.flux, 0, 3)]);
	g_key_file_set_integer(kf, section, "wait_delay",               config.delai);
	g_key_file_set_integer(kf, section, "wait_char",                config.car);
	g_key_file_set_integer(kf, section, "rs485_rts_time_before_tx", config.rs485_rts_time_before_transmit);
	g_key_file_set_integer(kf, section, "rs485_rts_time_after_tx",  config.rs485_rts_time_after_transmit);
	g_key_file_set_boolean(kf, section, "echo",                     config.echo);
	g_key_file_set_boolean(kf, section, "crlfauto",                 config.crlfauto);
	g_key_file_set_boolean(kf, section, "autoreconnect_enabled",    config.autoreconnect_enabled);
	g_key_file_set_boolean(kf, section, "esc_clear_screen",         config.esc_clear_screen);
	g_key_file_set_boolean(kf, section, "timestamp",                config.timestamp);
	g_key_file_set_boolean(kf, section, "disable_hotkeys",          config.disable_hotkeys);

	font_raw = term_conf.font_desc
	    ? pango_font_description_to_string(term_conf.font_desc)
	    : NULL;

	font_quoted = g_shell_quote(font_raw ?: DEFAULT_FONT);
	g_key_file_set_string(kf, section, "font", font_quoted);
	g_free(font_quoted);
	g_free(font_raw);

	macros = get_shortcuts(&size);
	if (size > 0)
	{
		const gchar **macro_strs = g_new(const gchar *, size);
		gchar       **alloc      = g_new(gchar *, size);
		for (guint i = 0; i < size; i++)
		{
			alloc[i]      = g_strdup_printf("%s::%s", macros[i].shortcut, macros[i].action);
			macro_strs[i] = alloc[i];
		}
		g_key_file_set_string_list(kf, section, "macros", macro_strs, size);
		for (guint i = 0; i < size; i++)
			g_free(alloc[i]);
		g_free(alloc);
		g_free(macro_strs);
	}
	else
		g_key_file_remove_key(kf, section, "macros", NULL);

	g_key_file_set_boolean(kf, section, "term_block_cursor",       term_conf.block_cursor);
	g_key_file_set_integer(kf, section, "term_rows",               term_conf.rows);
	g_key_file_set_integer(kf, section, "term_columns",            term_conf.columns);
	g_key_file_set_integer(kf, section, "term_scrollback",         term_conf.scrollback);
	g_key_file_set_boolean(kf, section, "term_visual_bell",        term_conf.visual_bell);

	for (gsize ci = 0; ci < G_N_ELEMENTS(color_fields); ci++)
	{
		g_autofree gchar *_s = g_strdup_printf("%.4f", *color_fields[ci].val);
		g_key_file_set_string(kf, section, color_fields[ci].key, _s);
	}
}

/* ------------------------------------------------------------------ */
/* Load a named section into config/term_conf                         */
/* ------------------------------------------------------------------ */

gint Load_configuration_from_file(const gchar *config_name)
{
	GKeyFile  *kf = get_key_file();
	gchar    **macro_vals;
	gchar     *s;
	gsize      n_macros;
	gint       v;

	if (!g_key_file_has_group(kf, config_name))
	{
		g_autofree gchar *msg = g_strdup_printf(_("No section \"%s\" in configuration file\n"), config_name);
		show_message(msg, MSG_ERR);
		return -1;
	}

	Hard_default_configuration();

	s = g_key_file_get_string(kf, config_name, "port", NULL);
	if (s) { g_strlcpy(config.port, s, sizeof(config.port)); g_free(s); }

	v = g_key_file_get_integer(kf, config_name, "speed", NULL);
	if (v) config.vitesse = (guint)v;

	v = g_key_file_get_integer(kf, config_name, "bits", NULL);
	if (v) config.bits = v;

	v = g_key_file_get_integer(kf, config_name, "stopbits", NULL);
	if (v) config.stops = v;

	s = g_key_file_get_string(kf, config_name, "parity", NULL);
	if (s)
	{
		if      (!g_ascii_strcasecmp(s, "odd"))  config.parite = 1;
		else if (!g_ascii_strcasecmp(s, "even")) config.parite = 2;
		else                                     config.parite = 0;
		g_free(s);
	}

	s = g_key_file_get_string(kf, config_name, "flow", NULL);
	if (s)
	{
		if      (!g_ascii_strcasecmp(s, "xon"))   config.flux = 1;
		else if (!g_ascii_strcasecmp(s, "rts"))   config.flux = 2;
		else if (!g_ascii_strcasecmp(s, "rs485")) config.flux = 3;
		else                                      config.flux = 0;
		g_free(s);
	}

	config.delai = g_key_file_get_integer(kf, config_name, "wait_delay", NULL);

	v = g_key_file_get_integer(kf, config_name, "wait_char", NULL);
	config.car = v ? (signed char)v : -1;

	config.rs485_rts_time_before_transmit =
	    g_key_file_get_integer(kf, config_name, "rs485_rts_time_before_tx", NULL);
	config.rs485_rts_time_after_transmit =
	    g_key_file_get_integer(kf, config_name, "rs485_rts_time_after_tx", NULL);

	config.echo                  = kf_get_bool(kf, config_name, "echo",                  FALSE);
	config.crlfauto              = kf_get_bool(kf, config_name, "crlfauto",              FALSE);
	config.autoreconnect_enabled = kf_get_bool(kf, config_name, "autoreconnect_enabled", FALSE);
	config.esc_clear_screen      = kf_get_bool(kf, config_name, "esc_clear_screen",      FALSE);
	config.timestamp             = kf_get_bool(kf, config_name, "timestamp",             FALSE);
	config.disable_hotkeys       = kf_get_bool(kf, config_name, "disable_hotkeys",       FALSE);

	s = g_key_file_get_string(kf, config_name, "font", NULL);
	if (s) {
		/* Old gtkterm stored font with literal shell-quote chars; unquote if needed. */
		gchar *font_str = g_shell_unquote(s, NULL);
		set_terminal_font_from_string(font_str ? font_str : s);
		g_free(font_str);
		g_free(s);
	}

	/* Macros — each entry is "shortcut::action" in a semicolon-separated list.
	 * Note: old config files stored multiple "macros =" lines; GKeyFile only
	 * keeps the last one, so existing multi-macro configs need one re-save. */
	n_macros   = 0;
	macro_vals = g_key_file_get_string_list(kf, config_name, "macros", &n_macros, NULL);
	if (macro_vals && n_macros > 0)
	{
		macro_t *macros = g_new(macro_t, n_macros);
		gsize n = 0;
		for (gsize mi = 0; mi < n_macros; mi++)
		{
			gchar *sep = strstr(macro_vals[mi], "::");
			if (sep)
			{
				macros[n].shortcut = g_strndup(macro_vals[mi], sep - macro_vals[mi]);
				macros[n].action   = g_strdup(sep + 2);
				n++;
			}
		}
		g_strfreev(macro_vals);
		create_shortcuts(macros, n); /* transfers ownership */
	}

	term_conf.block_cursor = kf_get_bool(kf, config_name, "term_block_cursor", TRUE);

	v = g_key_file_get_integer(kf, config_name, "term_rows", NULL);
	if (v) term_conf.rows = v;

	v = g_key_file_get_integer(kf, config_name, "term_columns", NULL);
	if (v) term_conf.columns = v;

	v = g_key_file_get_integer(kf, config_name, "term_scrollback", NULL);
	if (v) term_conf.scrollback = v;

	term_conf.visual_bell = kf_get_bool(kf, config_name, "term_visual_bell", FALSE);

	term_conf.foreground_color.red   = kf_get_color(kf, config_name, "term_foreground_red",   0.66f);
	term_conf.foreground_color.green = kf_get_color(kf, config_name, "term_foreground_green", 0.66f);
	term_conf.foreground_color.blue  = kf_get_color(kf, config_name, "term_foreground_blue",  0.66f);
	term_conf.foreground_color.alpha = kf_get_color(kf, config_name, "term_foreground_alpha", 1.0f);

	term_conf.background_color.red   = kf_get_color(kf, config_name, "term_background_red",   0.0f);
	term_conf.background_color.green = kf_get_color(kf, config_name, "term_background_green", 0.0f);
	term_conf.background_color.blue  = kf_get_color(kf, config_name, "term_background_blue",  0.0f);
	term_conf.background_color.alpha = kf_get_color(kf, config_name, "term_background_alpha", 1.0f);

	/* If rows/columns missing, reset terminal dimensions to defaults */
	if (term_conf.rows == 0 || term_conf.columns == 0)
	{
		term_conf.block_cursor = TRUE;
		term_conf.rows = 80;
		term_conf.columns = 25;
		term_conf.scrollback = DEFAULT_SCROLLBACK;
		term_conf.visual_bell = FALSE;
		Selec_couleur(&term_conf.foreground_color, 0.66f, 0.66f, 0.66f, 1.0f);
		Selec_couleur(&term_conf.background_color, 0, 0, 0, 1.0f);
	}

	vte_terminal_set_size(VTE_TERMINAL(display), term_conf.rows, term_conf.columns);
	vte_terminal_set_scrollback_lines(VTE_TERMINAL(display), term_conf.scrollback);
	vte_terminal_set_color_foreground(VTE_TERMINAL(display), &term_conf.foreground_color);
	vte_terminal_set_color_background(VTE_TERMINAL(display), &term_conf.background_color);
	vte_terminal_set_cursor_shape(VTE_TERMINAL(display),
	    term_conf.block_cursor ? VTE_CURSOR_SHAPE_BLOCK : VTE_CURSOR_SHAPE_IBEAM);
	gtk_widget_queue_draw(display);

	return 0;
}

/* ------------------------------------------------------------------ */
/* Validate the loaded configuration; warn on out-of-range values     */
/* ------------------------------------------------------------------ */

void Verify_configuration(void)
{
	gchar *string = NULL;

	if (find_standard_baudrate(config.vitesse) == B0)
	{
		string = g_strdup_printf(_("Baud rate %u may not be supported by all hardware"), config.vitesse);
		show_message(string, MSG_ERR);
		g_free(string);
	}

	if (config.stops != 1 && config.stops != 2)
	{
		string = g_strdup_printf(_("Invalid number of stop-bits: %d\nFalling back to default number of stop-bits number: %d\n"), config.stops, DEFAULT_STOP);
		show_message(string, MSG_ERR);
		config.stops = DEFAULT_STOP;
		g_free(string);
	}

	if (config.bits < 5 || config.bits > 8)
	{
		string = g_strdup_printf(_("Invalid number of bits: %d\nFalling back to default number of bits: %d\n"), config.bits, DEFAULT_BITS);
		show_message(string, MSG_ERR);
		config.bits = DEFAULT_BITS;
		g_free(string);
	}

	if (config.delai < 0 || config.delai > 500)
	{
		string = g_strdup_printf(_("Invalid delay: %d ms\nFalling back to default delay: %d ms\n"), config.delai, DEFAULT_DELAY);
		show_message(string, MSG_ERR);
		config.delai = DEFAULT_DELAY;
		g_free(string);
	}

	if (term_conf.font_desc == NULL)
		set_terminal_font_from_string(DEFAULT_FONT);
}

/* ------------------------------------------------------------------ */
/* Bootstrap: ensure a [default] section exists                       */
/* ------------------------------------------------------------------ */

gint Check_configuration_file(void)
{
	GKeyFile *kf = get_key_file();

	if (g_key_file_has_group(kf, "default"))
	{
		if (Load_configuration_from_file("default") == -1)
		{
			Hard_default_configuration();
			return -1;
		}
	}
	else
	{
		gchar *msg = g_strdup_printf(
		    _("Configuration file (%s) with\n[default] configuration has been created.\n"),
		    g_file_get_path(config_file));
		show_message(msg, MSG_WRN);
		g_free(msg);
		Hard_default_configuration();
		Copy_configuration(kf, "default");
		save_key_file();
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* Window geometry persistence — stored in the [window] section,      */
/* independent of named port-configuration sections.                  */
/* ------------------------------------------------------------------ */

#define WINDOW_SECTION "window"

void save_window_geometry(void)
{
	int width;
	int height;
        GKeyFile *kf;

        if (!Fenetre)
                return;

        width  = gtk_widget_get_width(GTK_WIDGET(Fenetre));
        height = gtk_widget_get_height(GTK_WIDGET(Fenetre));

        if (width <= 0 || height <= 0)
                return;

        kf = get_key_file();
        g_key_file_set_integer(kf, WINDOW_SECTION, "width",  width);
        g_key_file_set_integer(kf, WINDOW_SECTION, "height", height);
        save_key_file();
}

void load_window_geometry(void)
{
	GKeyFile *kf;
	int width;
	int height;
	GdkDisplay *gdk_disp;

	if (!Fenetre)
		return;

	kf = get_key_file();
	if (!g_key_file_has_group(kf, WINDOW_SECTION))
		return;

	width  = g_key_file_get_integer(kf, WINDOW_SECTION, "width",  NULL);
	height = g_key_file_get_integer(kf, WINDOW_SECTION, "height", NULL);

	if (width <= 0 || height <= 0)
		return;

	/* Clamp to the work area of the first monitor (excludes taskbars/panels) */
	gdk_disp = gdk_display_get_default();
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
				width  = CLAMP(width,  100, workarea.width);
				height = CLAMP(height, 100, workarea.height);
				g_object_unref(monitor);
			}
		}
	}

	gtk_window_set_default_size(GTK_WINDOW(Fenetre), width, height);
}
