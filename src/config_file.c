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
/*        - load and save GKeyFile objects on demand                   */
/*        - load / save sections to/from config & term_conf            */
/*        - hard-coded defaults                                        */
/*                                                                     */
/***********************************************************************/

#include <string.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "term_config.h"
#include "interface.h"
#include "config_file.h"
#include "macros.h"
#include "terminal_config.h"
#include "serial.h"

#define CONFIGURATION_FILENAME ".gtktermrc"

/* Default terminal colors */
static const GdkRGBA DEFAULT_FOREGROUND_COLOR = {0.66f, 0.66f, 0.66f, 1.0f};
static const GdkRGBA DEFAULT_BACKGROUND_COLOR = {0.0f, 0.0f, 0.0f, 1.0f};

/* Module-private globals */
static gchar             *config_path;

/* Public globals — forward-declared as extern in config_file.h */
struct configuration_port config;
display_config_t          term_conf;

/* ------------------------------------------------------------------ */
/* GKeyFile loading/saving                                             */
/* ------------------------------------------------------------------ */

/* Load the GKeyFile for the config file.
 * If the file does not exist an empty GKeyFile is returned — the caller
 * creates the content and saves it. */
static GKeyFile *load_key_file(void)
{
	GError *err  = NULL;
	GKeyFile *kf;

	kf = g_key_file_new();
	g_key_file_load_from_file(kf, config_path, G_KEY_FILE_KEEP_COMMENTS, &err);
	g_clear_error(&err); /* ignore "file not found" */
	return kf;
}

static gboolean save_key_file(GKeyFile *kf)
{
	GError *err = NULL;

	if (kf == NULL)
		return FALSE;

	gboolean ok = g_key_file_save_to_file(kf, config_path, &err);
	if (!ok)
	{
		show_messagef(MSG_ERR, _("Cannot save configuration file: %s"),
		              err ? err->message : _("unknown error"));
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
	gchar *old_path = g_build_filename(getenv("HOME"), CONFIGURATION_FILENAME, NULL);
	config_path = g_build_filename(g_get_user_config_dir(), CONFIGURATION_FILENAME, NULL);

	if (!g_file_test(config_path, G_FILE_TEST_EXISTS) &&
	     g_file_test(old_path, G_FILE_TEST_EXISTS))
		g_rename(old_path, config_path);
	g_free(old_path);
}

void config_file_free(void)
{
	g_free(config_path);
	config_path = NULL;
	g_free(term_conf.font);
	term_conf.font = NULL;
}

/* ------------------------------------------------------------------ */
/* Defaults                                                            */
/* ------------------------------------------------------------------ */

static void Hard_default_configuration(void)
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
	config.disable_port_lock = FALSE;

	term_conf.echo = DEFAULT_ECHO;
	term_conf.crlfauto = FALSE;
	term_conf.autoreconnect_enabled = FALSE;
	term_conf.esc_clear_screen = FALSE;
	term_conf.timestamp = FALSE;
	term_conf.disable_hotkeys = FALSE;

	g_free(term_conf.font);
	term_conf.font = NULL;  /* Will be set in Verify_configuration */
	term_conf.block_cursor = TRUE;
	term_conf.rows = 80;
	term_conf.columns = 25;
	term_conf.scrollback = DEFAULT_SCROLLBACK;
	term_conf.visual_bell = TRUE;

	term_conf.foreground_color = DEFAULT_FOREGROUND_COLOR;
	term_conf.background_color = DEFAULT_BACKGROUND_COLOR;
}

/* ------------------------------------------------------------------ */
/* Copy current config/term_conf into a GKeyFile section              */
/* ------------------------------------------------------------------ */

static void Copy_configuration(GKeyFile *kf, const gchar *section)
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
	gchar   *font_quoted;
	macro_t *macros;
	gsize    size;
	gsize    ci;

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
	g_key_file_set_boolean(kf, section, "echo",                     term_conf.echo);
	g_key_file_set_boolean(kf, section, "crlfauto",                 term_conf.crlfauto);
	g_key_file_set_boolean(kf, section, "autoreconnect_enabled",    term_conf.autoreconnect_enabled);
	g_key_file_set_boolean(kf, section, "esc_clear_screen",         term_conf.esc_clear_screen);
	g_key_file_set_boolean(kf, section, "timestamp",                term_conf.timestamp);
	g_key_file_set_boolean(kf, section, "disable_hotkeys",          term_conf.disable_hotkeys);

	font_quoted = g_shell_quote(term_conf.font ? term_conf.font : DEFAULT_FONT);
	g_key_file_set_string(kf, section, "font", font_quoted);
	g_free(font_quoted);

	macros = get_shortcuts(&size);
	if (size > 0)
	{
		const gchar **macro_strs = g_new(const gchar *, size);
		gchar       **alloc      = g_new(gchar *, size);
		guint i;
		for (i = 0; i < size; i++)
		{
			alloc[i]      = g_strdup_printf("%s::%s", macros[i].shortcut, macros[i].action);
			macro_strs[i] = alloc[i];
		}
		g_key_file_set_string_list(kf, section, "macros", macro_strs, size);
		for (i = 0; i < size; i++)
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

	for (ci = 0; ci < G_N_ELEMENTS(color_fields); ci++)
	{
		g_autofree gchar *_s = g_strdup_printf("%.4f", *color_fields[ci].val);
		g_key_file_set_string(kf, section, color_fields[ci].key, _s);
	}

	if (term_conf.window_width > 0 && term_conf.window_height > 0)
	{
		g_key_file_set_integer(kf, section, "window_width",  term_conf.window_width);
		g_key_file_set_integer(kf, section, "window_height", term_conf.window_height);
	}
}

/* ------------------------------------------------------------------ */
/* Normalise configuration values to valid ranges after any load       */
/* ------------------------------------------------------------------ */

static void Verify_configuration(void)
{
	if (config.stops != 1 && config.stops != 2)
		config.stops = DEFAULT_STOP;

	if (config.bits < 5 || config.bits > 8)
		config.bits = DEFAULT_BITS;

	if (config.parite < 0 || config.parite > 2)
		config.parite = DEFAULT_PARITY;

	if (config.delai < 0 || config.delai > 500)
		config.delai = DEFAULT_DELAY;

	if (config.rs485_rts_time_before_transmit < 0 || config.rs485_rts_time_before_transmit > 500)
		config.rs485_rts_time_before_transmit = DEFAULT_DELAY_RS485;

	if (config.rs485_rts_time_after_transmit < 0 || config.rs485_rts_time_after_transmit > 500)
		config.rs485_rts_time_after_transmit = DEFAULT_DELAY_RS485;

	if (term_conf.font == NULL)
		term_conf.font = g_strdup(DEFAULT_FONT);
}

/* ------------------------------------------------------------------ */
/* Load a named section into config/term_conf                         */
/* ------------------------------------------------------------------ */

gint Load_configuration_from_file(const gchar *config_name)
{
	GKeyFile  *kf = load_key_file();
	gchar    **macro_vals;
	gchar     *s;
	gsize      n_macros;
	gint       v;

	if (!g_key_file_has_group(kf, config_name))
	{
		g_key_file_free(kf);
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

	term_conf.echo                  = kf_get_bool(kf, config_name, "echo",                  FALSE);
	term_conf.crlfauto              = kf_get_bool(kf, config_name, "crlfauto",              FALSE);
	term_conf.autoreconnect_enabled = kf_get_bool(kf, config_name, "autoreconnect_enabled", FALSE);
	term_conf.esc_clear_screen      = kf_get_bool(kf, config_name, "esc_clear_screen",      FALSE);
	term_conf.timestamp             = kf_get_bool(kf, config_name, "timestamp",             FALSE);
	term_conf.disable_hotkeys       = kf_get_bool(kf, config_name, "disable_hotkeys",       FALSE);

	s = g_key_file_get_string(kf, config_name, "font", NULL);
	if (s) {
		/* Old gtkterm stored font with literal shell-quote chars; unquote if needed. */
		gchar *font_str = g_shell_unquote(s, NULL);
		term_conf.font = g_strdup(font_str ? font_str : s);
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
		gsize mi;
		for (mi = 0; mi < n_macros; mi++)
		{
			gchar *sep = strstr(macro_vals[mi], "::");
			if (sep)
			{
				macros[n].shortcut = g_strndup(macro_vals[mi], sep - macro_vals[mi]);
				macros[n].action   = g_strdup(sep + 2);
				n++;
			}
		}
		create_shortcuts(macros, n); /* transfers ownership */
	}
	g_strfreev(macro_vals); /* safe when NULL; frees even if n_macros == 0 */

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
		term_conf.foreground_color = DEFAULT_FOREGROUND_COLOR;
		term_conf.background_color = DEFAULT_BACKGROUND_COLOR;
	}

	v = g_key_file_get_integer(kf, config_name, "window_width",  NULL);
	if (v > 0) term_conf.window_width = v;
	v = g_key_file_get_integer(kf, config_name, "window_height", NULL);
	if (v > 0) term_conf.window_height = v;

	g_key_file_free(kf);

	Verify_configuration();
	return 0;
}

gboolean Save_configuration_to_file(const gchar *config_name)
{
	GKeyFile *kf = load_key_file();
	Copy_configuration(kf, config_name);
	gboolean ok = save_key_file(kf);
	g_key_file_free(kf);
	return ok;
}

/* ------------------------------------------------------------------ */
/* Query whether a named section exists in the config file            */
/* ------------------------------------------------------------------ */

gboolean config_section_exists(const gchar *config_name)
{
	GKeyFile *kf = load_key_file();
	gboolean  exists = g_key_file_has_group(kf, config_name);
	g_key_file_free(kf);
	return exists;
}

gboolean config_delete_section(const gchar *config_name)
{
	GError   *err = NULL;
	GKeyFile *kf  = load_key_file();
	gboolean  ok  = g_key_file_remove_group(kf, config_name, &err);
	if (ok)
		save_key_file(kf);
	g_clear_error(&err);
	g_key_file_free(kf);
	return ok;
}

gchar **config_get_sections(void)
{
	GKeyFile *kf = load_key_file();
	gchar   **groups = g_key_file_get_groups(kf, NULL);
	g_key_file_free(kf);
	return groups;
}

/* ------------------------------------------------------------------ */
/* Bootstrap: ensure a [default] section exists                       */
/* ------------------------------------------------------------------ */

void Check_configuration_file(void)
{
	Hard_default_configuration();
	Load_configuration_from_file("default");
}
