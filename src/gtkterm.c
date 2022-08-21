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
#include "gtkterm.h"
#include "gtkterm_window.h"
#include "terminal.h"
#include "cmdline.h"
#include "interface.h"
#include "serial.h"

unsigned int gtkterm_signals[LAST_GTKTERM_SIGNAL];

G_DEFINE_TYPE (GtkTerm, gtkterm, GTK_TYPE_APPLICATION)

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

  //! Clean up memory
  g_free (app->section);

  //! \todo: Should be part of the Gtkterm application struct
  g_option_group_unref (app->g_term_group);
  g_option_group_unref (app->g_port_group);
  g_option_group_unref (app->g_config_group); 
}

static GActionEntry gtkterm_entries[] = {
  { "quit", on_gtkterm_quit, NULL, NULL, NULL },

};

static void gtkterm_startup (GApplication *app) {

  GtkBuilder *builder;

  G_APPLICATION_CLASS (gtkterm_parent_class)->startup (app);

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/com/github/jeija/gtkterm/menu.ui", NULL);

  gtk_application_set_menubar (GTK_APPLICATION (app),
                               G_MENU_MODEL (gtk_builder_get_object (builder, "gtkterm_menubar")));

  g_object_unref (builder);
}

static void gtkterm_activate (GApplication *app) {

  //! Create the gtkterm_window
  create_window (app);
}

// Do the basic initialization of the application
static void gtkterm_init (GtkTerm *app) {
  GSettings *settings;

  settings = g_settings_new ("com.github.jeija.gtkterm");

  //! Set an action group for the app entries.
  app->action_group =  G_ACTION_GROUP (g_simple_action_group_new ()); 

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   gtkterm_entries, 
                                   G_N_ELEMENTS (gtkterm_entries),
                                   app);  

  //! load the config file and set the section to [default]
  app->config = GTKTERM_CONFIGURATION (g_object_new (GTKTERM_TYPE_CONFIGURATION, NULL));
  app->section = g_strdup (DEFAULT_SECTION);

  gtkterm_add_cmdline_options (app); 

  g_object_unref (settings);
}

static void gtkterm_class_init (GtkTermClass *class) {

  GApplicationClass *app_class = G_APPLICATION_CLASS (class); 

  gtkterm_signals[SIGNAL_GTKTERM_LOAD_CONFIG] = g_signal_new ("config_load",
                                                GTKTERM_TYPE_CONFIGURATION,
                                                G_SIGNAL_RUN_FIRST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_NONE,
                                                0,
                                                NULL);

  gtkterm_signals[SIGNAL_GTKTERM_SAVE_CONFIG] = g_signal_new ("config_save",
                                                GTKTERM_TYPE_CONFIGURATION,
                                                G_SIGNAL_RUN_FIRST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_NONE,
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
                                               G_TYPE_NONE,
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
                                               G_TYPE_NONE,
                                               3,
																               G_TYPE_POINTER,
																               G_TYPE_POINTER,
																               G_TYPE_POINTER,                                                                                                                                             
                                               NULL);                                               

  app_class->startup = gtkterm_startup;
  app_class->activate = gtkterm_activate;
}

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
