
#ifndef GTKTERM_H
#define GTKTERM_H

#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "gtkterm_defaults.h"
#include "gtkterm_configuration.h"

/** The signals which are defined */
enum {
    SIGNAL_GTKTERM_LOAD_CONFIG,
    SIGNAL_GTKTERM_SAVE_CONFIG,
    SIGNAL_GTKTERM_LIST_CONFIG, 
    SIGNAL_GTKTERM_REMOVE_SECTION,
    SIGNAL_GTKTERM_PRINT_SECTION,
    SIGNAL_GTKTERM_COPY_SECTION,    
    SIGNAL_GTKTERM_CONFIG_TERMINAL,
    SIGNAL_GTKTERM_CONFIG_SERIAL,
    SIGNAL_GTKTERM_CONFIG_CHECK_FILE,  
    SIGNAL_GTKTERM_TERMINAL_CHANGED,
    SIGNAL_GTKTERM_SERIAL_CONNECT,
    SIGNAL_GTKTERM_SERIAL_DATA_RECEIVED,
    SIGNAL_GTKTERM_SERIAL_DATA_TRANSMIT,
    SIGNAL_GTKTERM_SERIAL_SIGNALS_CHANGED,
    SIGNAL_GTKTERM_BUFFER_UPDATED,
    LAST_GTKTERM_SIGNAL
};

extern unsigned int gtkterm_signals[];

G_BEGIN_DECLS

/**
 * @brief The main GtkTerm application class.
 * 
 * All application specific variables are defined here.
 */
struct _GtkTerm {

  GtkApplication parent_instance;

  GOptionGroup *g_term_group;
  GOptionGroup *g_port_group;
  GOptionGroup *g_config_group;

  GActionGroup *action_group;           //!< App action group

  GtkTermConfiguration *config;         //!< The Key file with the configurations
  char *section;                        //!< The section provided from the cli. 
};

#define GTKTERM_TYPE_APP gtkterm_get_type()
G_DECLARE_FINAL_TYPE (GtkTerm, gtkterm, GTKTERM, APP, GtkApplication)
typedef struct _GtkTerm GtkTerm;

G_END_DECLS

#endif // GTKTERM_H