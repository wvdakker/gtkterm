
#ifndef GTKTERM_H
#define GTKTERM_H

#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "resource_file.h"

enum {
    SIGNAL_GTKTERM_LOAD_CONFIG,
    SIGNAL_GTKTERM_SAVE_CONFIG,  
    SIGNAL_GTKTERM_REMOVE_SECTION,
    SIGNAL_GTKTERM_PRINT_SECTION,
    SIGNAL_GTKTERM_CONFIG_TERMINAL,
    SIGNAL_GTKTERM_CONFIG_SERIAL,
    SIGNAL_GTKTERM_TERMINAL_CHANGED,    
    LAST_GTKTERM_SIGNAL
};

extern unsigned int gtkterm_signals[];

G_BEGIN_DECLS

//! @brief The main GtkTerm application class.
//! All application specific variables are defined here.
struct _GtkTerm {

  GtkApplication parent_instance;

  GOptionGroup *g_term_group;
  GOptionGroup *g_port_group;
  GOptionGroup *g_config_group;

  GActionGroup *action_group;           //! App action group

  GtkTermConfiguration *config;         //! The Key file with the configurations
  char *section;                        //! The section provided from the cli. 
                                        //! Terminals have their own section pointer
};

#define GTKTERM_TYPE_APP gtkterm_get_type()
typedef struct _GtkTerm GtkTerm;
G_DECLARE_FINAL_TYPE (GtkTerm, gtkterm, GTKTERM, APP, GtkApplication)


#endif // GTKTERM_H