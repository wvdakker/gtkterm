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

#include <glib.h>
#include "term_config.h"

/* The live configuration structs, defined in config_file.c */
extern struct configuration_port config;
extern display_config_t          term_conf;

/* One-time initialisation: locate/migrate the config file path */
void         config_file_init(void);
void         config_file_free(void);

/* Populate config/term_conf with compiled-in defaults */

/* Save current config/term_conf to a named section in the config file */
gboolean     Save_configuration_to_file(const gchar *config_name);

/* Load, verify and check configuration */
gint     Load_configuration_from_file(const gchar *config_name);
void     Verify_configuration(void);
gint     Check_configuration_file(void);
gboolean config_section_exists(const gchar *config_name);
gchar  **config_get_sections(void);
gboolean config_delete_section(const gchar *config_name);



#endif /* CONFIG_FILE_H_ */
