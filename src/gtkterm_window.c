
#include <gtk/gtk.h>
#include <vte/vte.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "config.h"
#include "gtkterm.h"
#include "gtkterm_window.h"
#include "terminal.h"


//! @brief The main GtkTermWindow class.
//! MainWindow specific variables here.
struct _GtkTermWindow {
  GtkApplicationWindow parent_instance;

  GtkWidget *message;                   //! Message for the infobar
  GtkWidget *infobar;                   //! Infobar
  GtkBox *statusbox;                    //! Box for statusbar messages
  GtkBox *status_config;                //! Displays the actual used configuration
  GtkWidget *menubutton;                //! Toolbar
  GMenuModel *toolmenu;                 //! Menu
  GtkScrolledWindow *scrolled_window;   //! Make the terminal window scrolled
  GtkTermTerminal *terminal_window;     //! The terminal window
  GtkWidget *search_bar;                //! Searchbar 
  GActionGroup *action_group;           //! Window action group
  GtkWidget *status_config_message[3];
  GtkWidget *status_serial_signal[6];  
  GtkWidget *status_message;

  int width;
  int height;
  bool maximized;
  bool fullscreen;
} ;

G_DEFINE_TYPE (GtkTermWindow, gtkterm_window, GTK_TYPE_APPLICATION_WINDOW)

static void gtkterm_window_update_statusbar (GtkTermWindow *, gpointer, gpointer, int, gpointer);
static void config_status_bar (GtkTermWindow *);
static void update_statusbar (GtkTermWindow *, gpointer, gpointer, int);
void set_window_title (GtkTermWindow *, gpointer);

static void on_gtkterm_about (GSimpleAction *, GVariant *, gpointer);
static void on_gtkterm_toggle_state (GSimpleAction *, GVariant *, gpointer);
static void on_gtkterm_toggle_dark (GSimpleAction *, GVariant *, gpointer);

static char const *serial_signal[] = {"DTR", "RTS", "CTS", "CD", "DSR", "RI"};

static GActionEntry gtkterm_window_entries[] = {
  // { "clear_screen", on_gtkterm_clear_screen, NULL, NULL, NULL },
  // { "clear_scrollback", on_gtkterm_clear_scrollback, NULL, NULL, NULL },
  // { "send_raw_file", on_gtkterm_send_raw, NULL, NULL, NULL },  
  // { "save_raw_file", on_gtkterm_save_raw, NULL, NULL, NULL },
  // { "copy", on_gtkterm_copy, NULL, NULL, NULL },
  // { "paste", on_gtkterm_paste, NULL, NULL, NULL },  
  // { "select_all", on_gtkterm_select_all, NULL, NULL, NULL },
  // { "log_to_file", on_gtkterm_log_to_file, NULL, NULL, NULL },
  // { "log_resume", on_gtkterm_log_resume, NULL, NULL, NULL },
  // { "log_stop", on_gtkterm_log_stop, NULL, NULL, NULL },
  // { "log_clear", on_gtkterm_log_clear, NULL, NULL, NULL },
  // { "config_port", on_gtkterm_config_port, NULL, NULL, NULL },
  // { "conig_main_window", on_gtkterm_config_main_window, NULL, NULL, NULL },
  // { "local_echo", on_gtkterm_local_echo, NULL, NULL, NULL },
  // { "crlf_auto", on_gtkterm_crlf_auto, NULL, NULL, NULL },
  // { "timestamp", on_gtkterm_timestamp, NULL, NULL, NULL },
  // { "macro", on_gtkterm_macros, NULL, NULL, NULL },
  // { "load_config", on_gtkterm_load_config, NULL, NULL, NULL },
  // { "save_config", on_gtkterm_save_config, NULL, NULL, NULL },
  // { "remove_config", on_gtkterm_remove_config, NULL, NULL, NULL },
  // { "send_break", on_gtkterm_send_break, NULL, NULL, NULL },
  // { "open_port", on_gtkterm_open_port, NULL, NULL, NULL },
  // { "close_port", on_gtkterm_close_port, NULL, NULL, NULL },         
  // { "toggle_DTR", on_gtkterm_toggle_DTR, NULL, NULL, NULL },
  // { "toggle_RTS", on_gtkterm_toggle_RTS, NULL, NULL, NULL },
  // { "show_ascii", on_gtkterm_show_ascii, NULL, NULL, NULL },
  // { "show_hex", on_gtkterm_show_hex, NULL, NULL, NULL },
  // { "view_hex", on_gtkterm_view_hex, NULL, NULL, NULL },
  // { "show_index", on_gtkterm_show_index, NULL, NULL, NULL },
  // { "send_hex_data,", on_gtkterm_send_hex_data, NULL, NULL, NULL },  
  { "dark", on_gtkterm_toggle_state, NULL, "false", on_gtkterm_toggle_dark}
};

static GActionEntry win_entries[] = {
  { "about", on_gtkterm_about, NULL, NULL, NULL }
};

void create_window (GApplication *app) {

  GtkTermWindow *window = (GtkTermWindow *)g_object_new (gtkterm_window_get_type (),
                                                          "application", 
                                                          GTKTERM_APP(app),
                                                          "show-menubar", 
                                                          TRUE,
                                                          NULL);

  //! Create a new terminal window and send section and keyfile as parameter
  //! GTKTERM_TERMINAL then can load the right section.
  window->terminal_window = gtkterm_terminal_new (GTKTERM_APP(app)->section, GTKTERM_APP(app), window);

  //! Make the VTE window scrollable
  gtk_scrolled_window_set_child(window->scrolled_window, GTK_WIDGET(window->terminal_window));
  
  gtk_window_present (GTK_WINDOW (window));
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

static void open_response_cb (GtkNativeDialog *dialog,
                  int              response_id,
                  gpointer         user_data) {

  GtkFileChooserNative *native = user_data;
  GtkWidget *message_dialog;
  GFile *file;
  char *contents;
  GError *error = NULL;

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (native));

      if (g_file_load_contents (file, NULL, &contents, NULL, NULL, &error))
        {
 //         create_window (app);
 //         g_free (contents);
        }
      else
        {
          message_dialog = gtk_message_dialog_new (NULL,
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   "Error loading file: \"%s\"",
                                                   error->message);
          g_signal_connect (message_dialog, "response", G_CALLBACK (gtk_window_destroy), NULL);
          gtk_widget_show (message_dialog);
          g_error_free (error);
        }
    }

  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (native));
  g_object_unref (native);
}

static void on_gtkterm_send_raw (GSimpleAction *action,
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
  g_signal_connect (native, "response", G_CALLBACK (open_response_cb), native);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (native));
}

static void on_gtkterm_toggle_state (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data) {
  GVariant *state;

//  show_action_dialog (action);

  state = g_action_get_state (G_ACTION (action));
  g_action_change_state (G_ACTION (action), g_variant_new_boolean (!g_variant_get_boolean (state)));
  g_variant_unref (state);
}

static void on_gtkterm_toggle_radio (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data) {

  show_action_infobar (action, parameter, user_data);

  g_action_change_state (G_ACTION (action), parameter);
}

static void on_gtkterm_about (GSimpleAction *action,
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


void config_status_bar (GtkTermWindow *window) {
    GtkWidget *label;

    //! Fields for the configuration and port
    for (int i = 0; i < 3; i++) {
        label = gtk_label_new ("");
        gtk_box_append (GTK_BOX (window->status_config), label);
        gtk_widget_set_halign (GTK_WIDGET (label), GTK_ALIGN_START);
        gtk_label_set_width_chars (GTK_LABEL(label), 25);
        window->status_config_message[i] = label;
    }

    //! Fill in the serial signals
    //! The signals are appended at the statusbox so they can glide along when resizing the window
    for (int i = 0; i < 6; i++) {
        label = gtk_label_new (serial_signal[i]);
        gtk_box_append (GTK_BOX (window->statusbox), label);
        gtk_widget_set_sensitive (GTK_WIDGET (label), FALSE);
        gtk_widget_set_halign (GTK_WIDGET (label), GTK_ALIGN_END);
        window->status_serial_signal[i] = label;
    }
}

static void update_statusbar (GtkTermWindow *window, gpointer section, gpointer serial_config_string, int serial_status) {
  char *msg;

  msg = g_strdup_printf ("[%s]", (char *)section);

  gtk_label_set_text (GTK_LABEL(window->status_config_message[0]), msg);
  gtk_label_set_label (GTK_LABEL(window->status_config_message[1]), serial_config_string);
  gtk_label_set_label (GTK_LABEL(window->status_config_message[2]), serial_status ? "connected" : "disconnected"); 

  g_free (msg);  
}

void set_window_title (GtkTermWindow *window, gpointer serial_config_string) {

  char *msg;

  msg = g_strdup_printf ("GtkTerm - %s", (char *)serial_config_string);

  gtk_window_set_title (GTK_WINDOW(window), msg);

  g_free (msg);  
}

//! Update the window title and statusbar with the new configuration
static void gtkterm_window_update_statusbar (GtkTermWindow *window, gpointer section, gpointer serial_config_string, int serial_status, gpointer user_data) {

  set_window_title (window, serial_config_string);
  update_statusbar (window, section, serial_config_string, serial_status);
}

static void on_gtkterm_toggle_dark (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data) {

  GtkSettings *settings = gtk_settings_get_default ();

  g_object_set (G_OBJECT (settings),
                "gtk-application-prefer-dark-theme",
                g_variant_get_boolean (state),
                NULL);

  g_simple_action_set_state (action, state);
}

static void on_gtkterm_toggle_radio_state (GSimpleAction *action,
                    GVariant      *state,
                    gpointer       user_data) {

  g_simple_action_set_state (action, state);
}

static void clicked_cb (GtkWidget *widget, GtkTermWindow *window) {

  gtk_widget_hide (window->infobar);
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

  window->action_group = G_ACTION_GROUP(g_simple_action_group_new ());                                 

  //! TODO: Rename it.
  g_action_map_add_action_entries (G_ACTION_MAP (window->action_group),
                                   win_entries, 
                                   G_N_ELEMENTS (win_entries),
                                   window);
  gtk_widget_insert_action_group (GTK_WIDGET (window), "gtkterm_window", window->action_group);
  g_action_map_add_action_entries (G_ACTION_MAP (window->action_group),
                                   win_entries, 
                                   G_N_ELEMENTS (gtkterm_window_entries),
                                   window);

    //! we cannot configure the statusbar within the UI templates.
  //! This means we have to do 'it by hand' and store the GtkLabels for later use.
  config_status_bar (window);                               
}

static void gtkterm_window_constructed (GObject *object) {
  GtkTermWindow *window = (GtkTermWindow *)object;

  gtkterm_window_load_state (window);

  gtk_window_set_default_size (GTK_WINDOW (window), window->width, window->height); 

  //! Connect to the terminal_changed so we can update the statusbar and window title
  g_signal_connect (window, "terminal_changed", G_CALLBACK(gtkterm_window_update_statusbar), NULL);	 

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

  gtkterm_signals[SIGNAL_GTKTERM_TERMINAL_CHANGED] = g_signal_new ("terminal_changed",
                                            GTKTERM_TYPE_GTKTERM_WINDOW,
                                            G_SIGNAL_RUN_FIRST,
                                            0,
                                            NULL,
                                            NULL,
                                            NULL,
                                            G_TYPE_NONE,
                                            3,
                                            G_TYPE_POINTER,
                                            G_TYPE_POINTER,
                                            G_TYPE_INT,
                                            NULL);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/github/jeija/gtkterm/gtkterm_main.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, message);
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, menubutton);
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, toolmenu);  
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, infobar);
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, scrolled_window);    
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, statusbox); 
  gtk_widget_class_bind_template_child (widget_class, GtkTermWindow, status_config);   

  gtk_widget_class_bind_template_callback (widget_class, clicked_cb);
}