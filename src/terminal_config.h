#ifndef TERMINAL_CONFIG_H_
#define TERMINAL_CONFIG_H_

#include <gtk/gtk.h>

void Config_Terminal(GSimpleAction *action, GVariant *param, gpointer data);
void clear_scrollback(void);
void set_terminal_font(PangoFontDescription *desc);
void set_terminal_font_from_string(const gchar *s);

#endif /* TERMINAL_CONFIG_H_ */
