#include <gtk/gtk.h>
#include <vte/vte.h>
#include <glib/gi18n.h>

#include "term_config.h"
#include "config_file.h"
#include "interface.h"
#include "terminal_config.h"

extern GtkWidget *display;

static void config_color(GObject *source,
                         GdkRGBA *dest,
                         void (*vte_set)(VteTerminal *, const GdkRGBA *),
                         const gchar *prefix)
{
	const GdkRGBA *c;
	GKeyFile *kf;
	gchar key[48];

	c = gtk_color_dialog_button_get_rgba(GTK_COLOR_DIALOG_BUTTON(source));
	if (!c) return;
	*dest = *c;

	vte_set(VTE_TERMINAL(display), dest);
	gtk_widget_queue_draw(display);

	kf = get_key_file();
	g_snprintf(key, sizeof key, "%s_red",   prefix); g_key_file_set_double(kf, "default", key, dest->red);
	g_snprintf(key, sizeof key, "%s_green", prefix); g_key_file_set_double(kf, "default", key, dest->green);
	g_snprintf(key, sizeof key, "%s_blue",  prefix); g_key_file_set_double(kf, "default", key, dest->blue);
	g_snprintf(key, sizeof key, "%s_alpha", prefix); g_key_file_set_double(kf, "default", key, dest->alpha);
}

void set_terminal_font(PangoFontDescription *desc)
{
	pango_font_description_free(term_conf.font_desc);
	term_conf.font_desc = desc;
	if (display)
		vte_terminal_set_font(VTE_TERMINAL(display), term_conf.font_desc);
}

void set_terminal_font_from_string(const gchar *s)
{
	set_terminal_font(pango_font_description_from_string(s));
}

static void read_font_button(GObject *source, GParamSpec *pspec, gpointer data)
{
	const PangoFontDescription *desc =
	    gtk_font_dialog_button_get_font_desc(GTK_FONT_DIALOG_BUTTON(source));
	if (desc)
		set_terminal_font(pango_font_description_copy(desc));
}

static gboolean cursor_block(GtkSwitch *ToggleSwitch, gboolean state, gpointer data)
{
	term_conf.block_cursor = state;
	vte_terminal_set_cursor_shape(VTE_TERMINAL(display),
	    term_conf.block_cursor ? VTE_CURSOR_SHAPE_BLOCK : VTE_CURSOR_SHAPE_IBEAM);
	return FALSE;
}

static void config_fg_color(GObject *source, GParamSpec *pspec, gpointer data)
{
	config_color(source, &term_conf.foreground_color,
	             vte_terminal_set_color_foreground, "term_foreground");
}

static void config_bg_color(GObject *source, GParamSpec *pspec, gpointer data)
{
	config_color(source, &term_conf.background_color,
	             vte_terminal_set_color_background, "term_background");
}

static void scrollback_set(GtkAdjustment *Adjustment, gpointer data)
{
	term_conf.scrollback = (gint)gtk_adjustment_get_value(Adjustment);
	vte_terminal_set_scrollback_lines(VTE_TERMINAL(display), term_conf.scrollback);
}

void clear_scrollback(void)
{
	vte_terminal_set_scrollback_lines(VTE_TERMINAL(display), 0);
	vte_terminal_set_scrollback_lines(VTE_TERMINAL(display), term_conf.scrollback);
}

void Config_Terminal(GSimpleAction *action, GVariant *param, gpointer data)
{
	GtkBuilderCScope *scope;
	GtkBuilder *builder;
	GtkWidget *dialog;
	GtkWidget *cfg_terminal_font;
	GtkAdjustment *cfg_scrollback_lines;
	GtkWidget *cfg_block_cursor;
	GtkWidget *cfg_text_color;
	GtkWidget *cfg_background_color;

	scope = GTK_BUILDER_CSCOPE(gtk_builder_cscope_new());
	gtk_builder_cscope_add_callback_symbols(scope,
		"read_font_button",    G_CALLBACK(read_font_button),
		"scrollback_set",      G_CALLBACK(scrollback_set),
		"cursor_block",        G_CALLBACK(cursor_block),
		"config_fg_color",     G_CALLBACK(config_fg_color),
		"config_bg_color",     G_CALLBACK(config_bg_color),
		"gtk_window_destroy",  G_CALLBACK(gtk_window_destroy),
		NULL);

	builder = gtk_builder_new();
	gtk_builder_set_scope(builder, GTK_BUILDER_SCOPE(scope));
	g_object_unref(scope);
	gtk_builder_add_from_resource(builder, "/org/gtk/gtkterm/config_terminal_dialog.ui", NULL);

	dialog = GTK_WIDGET(gtk_builder_get_object(builder, "dialog"));
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(Fenetre));

	cfg_terminal_font = GTK_WIDGET(gtk_builder_get_object(builder, "cfg_terminal_font"));
	gtk_font_dialog_button_set_font_desc(GTK_FONT_DIALOG_BUTTON(cfg_terminal_font), term_conf.font_desc);

	cfg_scrollback_lines = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "cfg_scrollback_lines"));
	gtk_adjustment_set_value(cfg_scrollback_lines, term_conf.scrollback);

	cfg_block_cursor = GTK_WIDGET(gtk_builder_get_object(builder, "cfg_block_cursor"));
	gtk_switch_set_active(GTK_SWITCH(cfg_block_cursor), term_conf.block_cursor);

	cfg_text_color = GTK_WIDGET(gtk_builder_get_object(builder, "cfg_text_color"));
	gtk_color_dialog_button_set_rgba(GTK_COLOR_DIALOG_BUTTON(cfg_text_color), &term_conf.foreground_color);

	cfg_background_color = GTK_WIDGET(gtk_builder_get_object(builder, "cfg_background_color"));
	g_object_unref(builder);

	gtk_window_present(GTK_WINDOW(dialog));
}
