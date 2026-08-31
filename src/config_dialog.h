#ifndef CONFIG_DIALOG_H_
#define CONFIG_DIALOG_H_

#include <gtk/gtk.h>

void select_config_callback(GSimpleAction *action, GVariant *param, gpointer data);
void save_config_callback(GSimpleAction *action, GVariant *param, gpointer data);
void delete_config_callback(GSimpleAction *action, GVariant *param, gpointer data);

#endif /* CONFIG_DIALOG_H_ */
