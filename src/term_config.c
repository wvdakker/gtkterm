#include <gtk/gtk.h>

#include "term_config.h"
#include "config_file.h"
#include "interface.h"

void ConfigFlags(void)
{
	Set_crlfauto(term_conf.crlfauto);
	Set_autoreconnect_enabled(term_conf.autoreconnect_enabled);
	Set_esc_clear_screen(term_conf.esc_clear_screen);
	Set_timestamp(term_conf.timestamp);
	Set_shortcuts_disabled(term_conf.disable_shortcuts);
}

/**
 *  Filter user data entry on a GTK entry
 *
 *  user_data must be a function that takes an int and returns an int
 *  != 0 if the input is valid.  For instance, 'isdigit()'.
 */
void check_text_input(GtkEditable *editable,
                      gchar       *new_text,
                      gint         new_text_length,
                      gint        *position G_GNUC_UNUSED,
                      gpointer     user_data)
{
	int i;
	int (*check_func)(int) = user_data;

	if(check_func == NULL)
		return;

	for(i = 0; i < new_text_length; i++)
	{
		if(!check_func((unsigned char)new_text[i]))
		{
			g_signal_stop_emission_by_name(editable, "insert-text");
			return;
		}
	}
}
