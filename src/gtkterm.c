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
#include "buffer.h"
#include "cmdline.h"
#include "interface.h"
#include "term_config.h"
#include "serial.h"

#define NR_OF_SIGNALS 4
unsigned int gtkterm_signals[NR_OF_SIGNALS];

#define GTKTERM_TYPE gtkterm_get_type ()
G_DEFINE_TYPE (GtkTerm, gtkterm, GTK_TYPE_APPLICATION)

#define GTKTERM_WINDOW_TYPE gtkterm_window_get_type ()
G_DEFINE_TYPE (GtkTermWindow, gtkterm_window, GTK_TYPE_APPLICATION_WINDOW)

static void update_statusbar (GtkTermWindow *);
void set_window_title (GtkTermWindow *);

static void create_window (GApplication *app) {

  GtkTermWindow *window = (GtkTermWindow *)g_object_new (gtkterm_window_get_type (),
                                                          "application", 
                                                          app,
                                                          "show-menubar", 
                                                          TRUE,
                                                          NULL);
  
  window->terminal_window = g_object_new (GTKTERM_TERMINAL_TYPE, NULL);

  gtk_scrolled_window_set_child(window->scrolled_window, GTK_WIDGET(window->terminal_window));

  set_window_title (window);

  //! TODO: update
  update_statusbar (window);

  gtk_window_present (GTK_WINDOW (window));
}

static void show_action_dialog (GSimpleAction *action)
{
  const char *name;
  GtkWidget *dialog;

  name = g_action_get_name (G_ACTION (action));

  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_CLOSE,
                                   "You activated action: \"%s\"",
                                    name);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (gtk_window_destroy), NULL);

  gtk_widget_show (dialog);
}

static void show_action_infobar (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       data) {

  GtkTermWindow *window = data;
  char *text;
  const char *name;
  const char *value;

  name = g_action_get_name (G_ACTION (action));
  value = g_variant_get_string (parameter, NULL);

  text = g_strdup_printf ("You activated radio action: \"%s\".\n"
                          "Current value: %s", name, value);
  gtk_label_set_text (GTK_LABEL (window->message), text);
  gtk_widget_show (window->infobar);
  g_free (text);
}

static void activate_action (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  show_action_dialog (action);
}

static void activate_new (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  GApplication *app = user_data;

  create_window (app);
}

static void open_response_cb (GtkNativeDialog *dialog,
                  int              response_id,
                  gpointer         user_data) {

  GtkFileChooserNative *native = user_data;
  GApplication *app = g_object_get_data (G_OBJECT (native), "gtkterm");
  GtkWidget *message_dialog;
  GFile *file;
  char *contents;
  GError *error = NULL;

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (native));

      if (g_file_load_contents (file, NULL, &contents, NULL, NULL, &error))
        {
          create_window (app);
          g_free (contents);
        }
      else
        {
          message_dialog = gtk_message_dialog_new (NULL,
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   "Error loading file: \"%s\"",
                                                   error->message);
          g_signal_connect (message_dialog, "response",
                            G_CALLBACK (gtk_window_destroy), NULL);
          gtk_widget_show (message_dialog);
          g_error_free (error);
        }
    }

  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (native));
  g_object_unref (native);
}

static void activate_open (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data) {

  GApplication *app = user_data;
  GtkFileChooserNative *native;

  native = gtk_file_chooser_native_new ("Open File",
                                        NULL,
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        "_Open",
                                        "_Cancel");

  g_object_set_data_full (G_OBJECT (native), "gtkterm", g_object_ref (app), g_object_unref);
  g_signal_connect (native,
                    "response",
                    G_CALLBACK (open_response_cb),
                    native);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (native));
}

static void activate_toggle (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data) {
  GVariant *state;

  show_action_dialog (action);

  state = g_action_get_state (G_ACTION (action));
  g_action_change_state (G_ACTION (action), g_variant_new_boolean (!g_variant_get_boolean (state)));
  g_variant_unref (state);
}

static void activate_radio (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data) {

  show_action_infobar (action, parameter, user_data);

  g_action_change_state (G_ACTION (action), parameter);
}

static void activate_about (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data) {

  GtkWidget *window = user_data;
  char *os_name;
  char *os_version;
  GString *s;

  const char *authors[] = {
    "Julien Schimtt", 
    "Zach Davis", 
    "Florian Euchner", 
    "Stephan Enderlein",
    "Kevin Picot",
    "Jens Georg",
    NULL};

  const char *comments =  _("GTKTerm is a simple GTK+ terminal used to communicate with the serial port.");

  s = g_string_new ("");

  os_name = g_get_os_info (G_OS_INFO_KEY_NAME);
  os_version = g_get_os_info (G_OS_INFO_KEY_VERSION_ID);

  if (os_name && os_version)
    g_string_append_printf (s, "OS\t%s %s\n\n", os_name, os_version);
  
  g_string_append (s, "System libraries\n");
  g_string_append_printf (s, "\tGLib\t%d.%d.%d\n",
                          glib_major_version,
                          glib_minor_version,
                          glib_micro_version);
  
  g_string_append_printf (s, "\tPango\t%s\n", pango_version_string ());
  g_string_append_printf (s, "\tGTK \t%d.%d.%d\n", gtk_get_major_version (),
                          gtk_get_minor_version (), gtk_get_micro_version ());

  GdkTexture *logo = gdk_texture_new_from_resource ("/com/github/jeija/gtkterm/gtkterm_48x48.png");

  gtk_show_about_dialog (GTK_WINDOW (window),
                         "program-name", "GTKTerm",
                         "version", PACKAGE_VERSION,
	                       "copyright", "Copyright © Julien Schimtt",
	                       "license-type", GTK_LICENSE_LGPL_3_0, 
	                       "website", "https://github.com/Jeija/gtkterm",
	                       "website-label", "https://github.com/Jeija/gtkterm",
                         "comments", "Program to demonstrate GTK functions.",
                         "authors", authors,
                         "logo-icon-name", "com.github.jeija.gtkterm",
                         "title", "GTKTerm",
                        "comments", comments,
	                       "logo", logo,
                         "system-information", s->str,
                         NULL);
}

static void activate_quit (GSimpleAction *action,
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

      g_free(GTKTERM_WINDOW(win)->terminal_window->filename);

      list = next;
    }

  //! TODO: Remove when GObject
  // delete_buffer();

  // Clean up memory
  g_free (app->initial_section);

  //! TODO: Should be part of the Gtkterm application struct
  g_option_group_unref (app->g_term_group);
  g_option_group_unref (app->g_port_group);
  g_option_group_unref (app->g_config_group); 
}

static void update_statusbar (GtkTermWindow *window) {
  //char *msg;

  /* clear any previous message, underflow is allowed */
  //gtk_statusbar_pop (GTK_STATUSBAR (window->status), 0);

  //msg = g_strdup_printf ("%s", get_port_string());

  //gtk_statusbar_push (GTK_STATUSBAR (window->status), 0, msg);

  //g_free (msg);
}

void set_window_title (GtkTermWindow *window) {

  char *msg;

  msg = g_strdup_printf ("GTKTerm - %s", get_port_string());
  gtk_window_set_title (GTK_WINDOW(window), msg);

  g_free (msg);
}

static void change_theme_state (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data) {

  GtkSettings *settings = gtk_settings_get_default ();

  g_object_set (G_OBJECT (settings),
                "gtk-application-prefer-dark-theme",
                g_variant_get_boolean (state),
                NULL);

  g_simple_action_set_state (action, state);
}

static void change_radio_state (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data) {

  g_simple_action_set_state (action, state);
}

static GActionEntry app_entries[] = {
  { "new", activate_new, NULL, NULL, NULL },
  { "open", activate_open, NULL, NULL, NULL },
  { "save", activate_action, NULL, NULL, NULL },
  { "save-as", activate_action, NULL, NULL, NULL },
  { "quit", activate_quit, NULL, NULL, NULL },
  { "dark", activate_toggle, NULL, "false", change_theme_state }
};

static GActionEntry win_entries[] = {
  { "shape", activate_radio, "s", "'oval'", change_radio_state },
  { "bold", activate_toggle, NULL, "false", NULL },
  { "about", activate_about, NULL, NULL, NULL },
  { "file1", activate_action, NULL, NULL, NULL },
  { "logo", activate_action, NULL, NULL, NULL }
};

static void clicked_cb (GtkWidget *widget, GtkTermWindow *window) {

  gtk_widget_hide (window->infobar);
}

static void startup (GApplication *app) {

  GtkBuilder *builder;

  G_APPLICATION_CLASS (gtkterm_parent_class)->startup (app);

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/com/github/jeija/gtkterm/menu.ui", NULL);

  gtk_application_set_menubar (GTK_APPLICATION (app),
                               G_MENU_MODEL (gtk_builder_get_object (builder, "gtkterm_menubar")));

  g_object_unref (builder);
}

static void activate (GApplication *app) {

  create_window (app);
}

// Do the basic initialization of the application
static void gtkterm_init (GtkTerm *app) {
  GSettings *settings;

  settings = g_settings_new ("com.github.jeija.gtkterm");

  app->config = GTKTERM_CONFIGURATION (g_object_new (GTKTERM_TYPE_CONFIGURATION, NULL));
  app->initial_section = g_strdup (DEFAULT_SECTION);

  g_signal_emit(app->config, gtkterm_signals[SIGNAL_LOAD_CONFIG], 0);

  //! TODO: Make GObject
  //! create_buffer();

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);

  gtkterm_add_cmdline_options (app); 

  g_object_unref (settings);
}

static void gtkterm_class_init (GtkTermClass *class) {

  GApplicationClass *app_class = G_APPLICATION_CLASS (class); 

  gtkterm_signals[SIGNAL_LOAD_CONFIG] = g_signal_new ("config_load",
                                                GTKTERM_TYPE_CONFIGURATION,
                                                G_SIGNAL_RUN_FIRST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_NONE,
                                               0,
                                               NULL);

  gtkterm_signals[SIGNAL_SAVE_CONFIG] = g_signal_new ("config_save",
                                                GTKTERM_TYPE_CONFIGURATION,
                                                G_SIGNAL_RUN_FIRST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_NONE,
                                                0,
                                                NULL);

  gtkterm_signals[SIGNAL_PRINT_SECTION] = g_signal_new ("config_print",
                                                 GTKTERM_TYPE_CONFIGURATION ,
                                                 G_SIGNAL_RUN_FIRST,
                                                 0,
                                                 NULL,
                                                 NULL,
                                                 NULL,
                                                 G_TYPE_NONE,
                                                 1,
                                                 G_TYPE_POINTER,
                                                 NULL);                                      

  gtkterm_signals[SIGNAL_REMOVE_SECTION] = g_signal_new ("config_remove",
                                                GTKTERM_TYPE_CONFIGURATION,
                                                G_SIGNAL_RUN_FIRST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_NONE,
                                                0,
                                                NULL);

  app_class->startup = startup;
  app_class->activate = activate;
}

static void gtkterm_window_store_state (GtkTermWindow *win) {

  GSettings *settings;

  settings = g_settings_new ("com.github.jeija.gtkterm");
  g_settings_set (settings, "window-size", "(ii)", win->width, win->height);
  g_settings_set_boolean (settings, "maximized", win->maximized);
  g_settings_set_boolean (settings, "fullscreen", win->fullscreen);
  g_object_unref (settings);
}

static void gtkterm_window_load_state (GtkTermWindow *win) {
  GSettings *settings;

  settings = g_settings_new ("com.github.jeija.gtkterm");
  g_settings_get (settings, "window-size", "(ii)", &win->width, &win->height);
  win->maximized = g_settings_get_boolean (settings, "maximized");
  win->fullscreen = g_settings_get_boolean (settings, "fullscreen");
  g_object_unref (settings);
}

static void gtkterm_window_init (GtkTermWindow *window) {
  
  GtkWidget *popover;

  window->width = -1;
  window->height = -1;
  window->maximized = FALSE;
  window->fullscreen = FALSE;

  gtk_widget_init_template (GTK_WIDGET (window));

  popover = gtk_popover_menu_new_from_model (window->toolmenu);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (window->menubutton), popover);

  g_action_map_add_action_entries (G_ACTION_MAP (window),
                                   win_entries, 
                                   G_N_ELEMENTS (win_entries),
                                   window);
}

static void gtkterm_window_constructed (GObject *object) {
  GtkTermWindow *window = (GtkTermWindow *)object;

  gtkterm_window_load_state (window);

  gtk_window_set_default_size (GTK_WINDOW (window), window->width, window->height);

  if (window->maximized)
    gtk_window_maximize (GTK_WINDOW (window));

  if (window->fullscreen)
    gtk_window_fullscreen (GTK_WINDOW (window));

  G_OBJECT_CLASS (gtkterm_window_parent_class)->constructed (object);
}

static void gtkterm_window_size_allocate (GtkWidget *widget,
                                       int width,
                                       int height,
                                       int baseline) {
                                        
  GtkTermWindow *window = (GtkTermWindow *)widget; 

  GTK_WIDGET_CLASS (gtkterm_window_parent_class)->size_allocate (widget,
                                                                          width,
                                                                          height,
                                                                          baseline);

  if (!window->maximized && !window->fullscreen)
    gtk_window_get_default_size (GTK_WINDOW (window), &window->width, &window->height);
}

static void surface_state_changed (GtkWidget *widget) {
  GtkTermWindow *window = (GtkTermWindow *)widget;
  GdkToplevelState new_state;

  new_state = gdk_toplevel_get_state (GDK_TOPLEVEL (gtk_native_get_surface (GTK_NATIVE (widget))));
  window->maximized = (new_state & GDK_TOPLEVEL_STATE_MAXIMIZED) != 0;
  window->fullscreen = (new_state & GDK_TOPLEVEL_STATE_FULLSCREEN) != 0;
}

static void gtkterm_window_realize (GtkWidget *widget) {
  GTK_WIDGET_CLASS (gtkterm_window_parent_class)->realize (widget);

  g_signal_connect_swapped (gtk_native_get_surface (GTK_NATIVE (widget)), "notify::state",
                            G_CALLBACK (surface_state_changed), widget);
}

static void gtkterm_window_unrealize (GtkWidget *widget) {
  g_signal_handlers_disconnect_by_func (gtk_native_get_surface (GTK_NATIVE (widget)),
                                        surface_state_changed, widget);

  GTK_WIDGET_CLASS (gtkterm_window_parent_class)->unrealize (widget);
}

static void gtkterm_window_dispose (GObject *object) {
  GtkTermWindow *window = (GtkTermWindow *)object;

  gtkterm_window_store_state (window);

  G_OBJECT_CLASS (gtkterm_window_parent_class)->dispose (object);
}

static void gtkterm_window_class_init (GtkTermWindowClass *class) {
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->constructed = gtkterm_window_constructed;
  object_class->dispose = gtkterm_window_dispose;

  widget_class->size_allocate = gtkterm_window_size_allocate;
  widget_class->realize = gtkterm_window_realize;
  widget_class->unrealize = gtkterm_window_unrealize;

  gtk_widget_class_set_template_from_resource (widget_class, "/com/github/jeija/gtkterm/gtkterm_main.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, message);
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, infobar);
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, status);
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, scrolled_window);   
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, menubutton);
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, toolmenu);
  gtk_widget_class_bind_template_callback (widget_class, clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, update_statusbar);
}

int main (int argc, char *argv[]) {
	GtkApplication *app;

	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	app = GTK_APPLICATION (g_object_new (GTKTERM_TYPE,
                                       "application-id", 
                                       "com.github.jeija.gtkterm",
                                       "flags", 
                                       G_APPLICATION_HANDLES_OPEN,
                                       NULL));                                    

	return g_application_run (G_APPLICATION (app), argc, argv);
}
