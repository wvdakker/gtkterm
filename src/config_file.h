/***********************************************************************/
/* config_file.h                                                       */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      GKeyFile-backed configuration file handling —                  */
/*      public API for loading, saving and managing .gtktermrc.        */
/*                                                                     */
/***********************************************************************/

#ifndef CONFIG_FILE_H_
#define CONFIG_FILE_H_

#include <gtk/gtk.h>
#include "term_config.h"

/* The GFile representing ~/.config/.gtktermrc */
extern GFile *config_file;

/* The live configuration structs, defined in config_file.c */
extern struct configuration_port config;
extern display_config_t          term_conf;

/* Load/save the configuration file on demand */
GKeyFile    *load_key_file(void);
gboolean     save_key_file(GKeyFile *kf);

/* One-time initialisation: locate/migrate the config file path */
void         config_file_init(void);
void         config_file_free(void);

/* Populate config/term_conf with compiled-in defaults */
void         Hard_default_configuration(void);

/* Write current config/term_conf into section of kf */
void         Copy_configuration(GKeyFile *kf, const gchar *section);

/* Load, verify and check configuration */
gint Load_configuration_from_file(const gchar *config_name);
void Verify_configuration(void);
gint Check_configuration_file(void);

/* Window geometry: auto-saved to [window] section, independent of named configs */
void save_window_geometry(int width, int height);
gboolean load_window_geometry(int *width, int *height);

#endif /* CONFIG_FILE_H_ */
