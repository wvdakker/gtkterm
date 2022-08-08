
#ifndef GTKTERM_H
#define GTKTERM_H

#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>

#include "terminal.h"
#include "resource_file.h"

enum {
    SIGNAL_LOAD_CONFIG,
    SIGNAL_SAVE_CONFIG,  
    SIGNAL_REMOVE_SECTION,
    SIGNAL_PRINT_SECTION
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

  GtkTermConfiguration *config;         //! The Key file with the configurations
  char *initial_section;                //! The initial section provided from the cli. 
                                        //! Terminals have their own section pointer

};

typedef struct _GtkTerm GtkTerm;
G_DECLARE_FINAL_TYPE (GtkTerm, gtkterm, GTKTERM, APP, GtkApplication)

//! @brief The main GtkTermWindow class.
//! MainWindow specific variables here.
struct _GtkTermWindow {
  GtkApplicationWindow parent_instance;

  GtkWidget *message;                   //! Message for the infobar
  GtkWidget *infobar;                   //! Infobar
  GtkWidget *status;                    //! Statusbar
  GtkWidget *menubutton;                //! Toolbar
  GMenuModel *toolmenu;                 //! Menu
  GtkScrolledWindow *scrolled_window;   //! Make the terminal window scrolled
  GtkTermTerminal *terminal_window;     //! The terminal window

  int width;
  int height;
  bool maximized;
  bool fullscreen;
  
} ;

typedef struct _GtkTermWindow GtkTermWindow ;
G_DECLARE_FINAL_TYPE (GtkTermWindow, gtkterm_window, GTKTERM, WINDOW, GtkApplicationWindow)

G_END_DECLS

#endif // GTKTERM_H