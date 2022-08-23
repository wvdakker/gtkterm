/***********************************************************************/
/* gtkterm.c                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Main program file                                              */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 2.0 : Ported to GTK4                                         */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.99.0 : added call to add_shortcuts()                       */
/*      - 0.98 : all GUI functions moved to widgets.c                  */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "config.h"
#include "defaults.h"
#include "gtkterm.h"
#include "gtkterm_window.h"
#include "terminal.h"
#include "cmdline.h"
#include "serial.h"

/** The gtkterm signals available */
unsigned int gtkterm_signals[LAST_GTKTERM_SIGNAL];

G_DEFINE_TYPE (GtkTerm, gtkterm, GTK_TYPE_APPLICATION)

/**
 * @brief Quitthe application
 * 
 * Is a callback function from the menubar and cleans up all
 * memory, windows etc.
 * 
 * @param action Not used.
 * 
 * @param parameter Not used.
 * 
 * @param user_data The application we want to quit
 * 
 */
static void on_gtkterm_quit (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data) {
 
  GtkTerm *app = GTKTERM_APP(user_data);
  GtkWidget *win;
  GList *list, *next;

  list = gtk_application_get_windows (GTK_APPLICATION(app));
  while (list)
    {
      win = list->data;
      next = list->next;

      gtk_window_destroy (GTK_WINDOW (win));

      list = next;
    }

  /** Clean up memory */
  g_free (app->section);

  /** \todo: Should be part of the Gtkterm application struct */
  g_option_group_unref (app->g_term_group);
  g_option_group_unref (app->g_port_group);
  g_option_group_unref (app->g_config_group); 
}

static GActionEntry gtkterm_entries[] = {
  { "quit", on_gtkterm_quit, NULL, NULL, NULL },

};

/**
 * @brief Startup the application
 * 
 * Initiaze the builder and add menu resources to the application
 * 
 * @param app The application
 * 
 */
static void gtkterm_startup (GApplication *app) {

  GtkBuilder *builder;

  G_APPLICATION_CLASS (gtkterm_parent_class)->startup (app);

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/com/github/jeija/gtkterm/menu.ui", NULL);

  gtk_application_set_menubar (GTK_APPLICATION (app),
                               G_MENU_MODEL (gtk_builder_get_object (builder, "gtkterm_menubar")));

  g_object_unref (builder);
}

/**
 * @brief Activates the application
 * 
 * Create the main window. The actual creation of the terminal will be done
 * in the GtkTermWindow file.
 * 
 * @param app The application
 * 
 *  \todo embed create_window in the gtkapplication window 
 * 
 */
static void gtkterm_activate (GApplication *app) {

  g_printf ("%p\n", GTKTERM_APP (app));
  GtkTermWindow *window = (GtkTermWindow *)g_object_new (GTKTERM_TYPE_GTKTERM_WINDOW,
                                                          "application", 
                                                          GTKTERM_APP(app),
                                                          "show-menubar", 
                                                          TRUE,
                                                          NULL);

  create_window (app, window);

  gtk_window_present (GTK_WINDOW (window));  
}

/**
 * @brief The initialization of the application
 * 
 * Setting the cli options and initialize the application variables
 * 
 * @param app The application
 * 
 */
static void gtkterm_init (GtkTerm *app) {
  GSettings *settings;

  settings = g_settings_new ("com.github.jeija.gtkterm");

  /** Set an action group for the app entries. */
  app->action_group =  G_ACTION_GROUP (g_simple_action_group_new ()); 

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   gtkterm_entries, 
                                   G_N_ELEMENTS (gtkterm_entries),
                                   app);  

  /** Initialize the config file and set the section to [default] */
  app->config = GTKTERM_CONFIGURATION (g_object_new (GTKTERM_TYPE_CONFIGURATION, NULL));
  app->section = g_strdup (DEFAULT_SECTION);

  gtkterm_add_cmdline_options (app); 

  g_object_unref (settings);
}

/**
 * @brief Initializing the application class
 * 
 * Setting the signals and callback functions
 * 
 * @param class The application class
 * 
 */
static void gtkterm_class_init (GtkTermClass *class) {

  GApplicationClass *app_class = G_APPLICATION_CLASS (class); 

  gtkterm_signals[SIGNAL_GTKTERM_LOAD_CONFIG] = g_signal_new ("config_load",
                                                GTKTERM_TYPE_CONFIGURATION,
                                                G_SIGNAL_RUN_FIRST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_POINTER,
                                                0,
                                                NULL);

  gtkterm_signals[SIGNAL_GTKTERM_SAVE_CONFIG] = g_signal_new ("config_save",
                                                GTKTERM_TYPE_CONFIGURATION,
                                                G_SIGNAL_RUN_FIRST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_POINTER,
                                                0,
                                                NULL);

  gtkterm_signals[SIGNAL_GTKTERM_PRINT_SECTION] = g_signal_new ("config_print",
                                                GTKTERM_TYPE_CONFIGURATION,
                                                G_SIGNAL_RUN_FIRST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_NONE,
                                                1,
                                                G_TYPE_POINTER,
                                                NULL);                                                                                                                                            

  gtkterm_signals[SIGNAL_GTKTERM_REMOVE_SECTION] = g_signal_new ("config_remove",
                                               GTKTERM_TYPE_CONFIGURATION,
                                               G_SIGNAL_RUN_FIRST,
                                               0,
                                               NULL,
                                               NULL,
                                               NULL,
                                               G_TYPE_POINTER,
                                               0,
                                               NULL);

    gtkterm_signals[SIGNAL_GTKTERM_CONFIG_TERMINAL] = g_signal_new ("config_terminal",
                                               GTKTERM_TYPE_CONFIGURATION,
                                               G_SIGNAL_RUN_FIRST,
                                               0,
                                             	 NULL,
                                               NULL,
                                               NULL,
                                               G_TYPE_POINTER,
                                               1,
																               G_TYPE_POINTER,
                                               NULL);

    gtkterm_signals[SIGNAL_GTKTERM_CONFIG_SERIAL] = g_signal_new ("config_serial",
                                               GTKTERM_TYPE_CONFIGURATION,
                                               G_SIGNAL_RUN_FIRST,
                                               0,
                                               NULL,
                                               NULL,
                                               NULL,
                                               G_TYPE_POINTER,
                                               1,
																               G_TYPE_POINTER,
                                               NULL);

  gtkterm_signals[SIGNAL_GTKTERM_COPY_SECTION] = g_signal_new ("config_copy",
                                               GTKTERM_TYPE_CONFIGURATION,
                                               G_SIGNAL_RUN_FIRST,
                                               0,
                                               NULL,
                                               NULL,
                                               NULL,
                                               G_TYPE_POINTER,
                                               3,
																               G_TYPE_POINTER,
																               G_TYPE_POINTER,
																               G_TYPE_POINTER,                                                                                                                                             
                                               NULL);                                               

  app_class->startup = gtkterm_startup;
  app_class->activate = gtkterm_activate;
}

/**
 * @brief The main function
 * 
 * @param argc Number of cli arguments
 * 
 * @param argv The cli arguments
 * 
 */
int main (int argc, char *argv[]) {
	GtkApplication *app;

	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	app = GTK_APPLICATION (g_object_new (GTKTERM_TYPE_APP,
                                       "application-id", 
                                       "com.github.jeija.gtkterm",
                                       "flags", 
                                       G_APPLICATION_HANDLES_OPEN,
                                       NULL));                                    

	return g_application_run (G_APPLICATION (app), argc, argv);
}
