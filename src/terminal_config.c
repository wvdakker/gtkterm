#include <gtk/gtk.h>
#include <vte/vte.h>
#include <glib/gi18n.h>

#include "term_config.h"
#include "config_file.h"
#include "interface.h"
#include "terminal_config.h"

static void config_color(GtkColorDialogButton *source,
                         GdkRGBA *dest,
                         void (*vte_set)(VteTerminal *, const GdkRGBA *))
{
	const GdkRGBA *c;

	c = gtk_color_dialog_button_get_rgba(source);
	if (!c) return;
	*dest = *c;

	vte_set(display, dest);
	gtk_widget_queue_draw(GTK_WIDGET(display));
}

static void read_font_button(GtkFontDialogButton *source, GParamSpec *pspec G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	const PangoFontDescription *desc =
	    gtk_font_dialog_button_get_font_desc(source);
	if (desc) {
		g_free(term_conf.font);
		term_conf.font = pango_font_description_to_string(desc);
		vte_terminal_set_font(display, desc);
	}
}

static gboolean cursor_block(GtkSwitch *ToggleSwitch G_GNUC_UNUSED, gboolean state, gpointer data G_GNUC_UNUSED)
{
	term_conf.block_cursor = state;
	vte_terminal_set_cursor_shape(display,
	    term_conf.block_cursor ? VTE_CURSOR_SHAPE_BLOCK : VTE_CURSOR_SHAPE_IBEAM);
	return FALSE;
}

static void config_fg_color(GtkColorDialogButton *source, GParamSpec *pspec G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	config_color(source, &term_conf.foreground_color,
	             vte_terminal_set_color_foreground);
}

static void config_bg_color(GtkColorDialogButton *source, GParamSpec *pspec G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	config_color(source, &term_conf.background_color,
	             vte_terminal_set_color_background);
}

static void scrollback_set(GtkAdjustment *Adjustment, gpointer data G_GNUC_UNUSED)
{
	term_conf.scrollback = (gint)gtk_adjustment_get_value(Adjustment);
	vte_terminal_set_scrollback_lines(display, term_conf.scrollback);
}

void clear_scrollback(void)
{
	vte_terminal_set_scrollback_lines(display, 0);
	vte_terminal_set_scrollback_lines(display, term_conf.scrollback);
}

void Config_Terminal(GSimpleAction *action G_GNUC_UNUSED, GVariant *param G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
	GtkBuilderCScope     *scope;
	GtkBuilder           *builder;
	GtkWindow            *dialog;
	GtkFontDialogButton  *cfg_terminal_font;
	GtkAdjustment        *cfg_scrollback_lines;
	GtkSwitch            *cfg_block_cursor;
	GtkColorDialogButton *cfg_text_color;
	GtkColorDialogButton *cfg_background_color;
	PangoFontDescription *desc;

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

	dialog = GTK_WINDOW(gtk_builder_get_object(builder, "dialog"));
	gtk_window_set_transient_for(dialog, GTK_WINDOW(Fenetre));

	cfg_terminal_font = GTK_FONT_DIALOG_BUTTON(gtk_builder_get_object(builder, "cfg_terminal_font"));

	desc = pango_font_description_from_string(term_conf.font);
	gtk_font_dialog_button_set_font_desc(cfg_terminal_font, desc);
	pango_font_description_free(desc);

	cfg_scrollback_lines = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "cfg_scrollback_lines"));
	gtk_adjustment_set_value(cfg_scrollback_lines, term_conf.scrollback);

	cfg_block_cursor = GTK_SWITCH(gtk_builder_get_object(builder, "cfg_block_cursor"));
	gtk_switch_set_active(cfg_block_cursor, term_conf.block_cursor);

	cfg_text_color = GTK_COLOR_DIALOG_BUTTON(gtk_builder_get_object(builder, "cfg_text_color"));
	gtk_color_dialog_button_set_rgba(cfg_text_color, &term_conf.foreground_color);

	cfg_background_color = GTK_COLOR_DIALOG_BUTTON(gtk_builder_get_object(builder, "cfg_background_color"));
	gtk_color_dialog_button_set_rgba(cfg_background_color, &term_conf.background_color);
	g_object_unref(builder);

	gtk_window_present(dialog);
}
