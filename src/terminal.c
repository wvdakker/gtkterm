/***********************************************************************/
/* terminal.c                                                          */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Handles all VTE in/output to/from serial port                  */
/*                                                                     */
/*   ChangeLog                                                         */

/***********************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <pango/pango-font.h>

#include "gtkterm_window.h"
#include "terminal.h"
#include "serial.h"
#include "resource_file.h"

typedef struct  {
    uint8_t view_mode;              //! ASCII or HEX view mode
 //   GtkTermBuffer *term_buffer;
    GtkTermSerialPort *serial_port;
    term_config_t *term_conf;       //! The configuration loaded from the keyfile
    port_config_t *port_conf;       //! Port configuration used in this terminal
 
    char *section;           		//! Section used in this terminal for configuration from config file
	GtkTerm *app;                   //! Pointer to the app for getting [section] and keyfile
    GtkTermWindow *main_window;     //! Pointer to the main window for updating the statusbar on changes

} GtkTermTerminalPrivate;

struct _GtkTermTerminal {

    VteTerminal vte_object;         //! The actual terminal
};

struct _GtkTermTerminalClass {

    VteTerminalClass vte_class; /// The vte class
};

G_DEFINE_TYPE_WITH_PRIVATE  (GtkTermTerminal, gtkterm_terminal, VTE_TYPE_TERMINAL)

enum { 
	PROP_0, 
	PROP_SECTION,
	PROP_GTKTERM_APP,
    PROP_MAIN_WINDOW,
	N_PROPS 
};

static GParamSpec *gtkterm_terminal_properties[N_PROPS] = {NULL};

GtkTermTerminal *gtkterm_terminal_new (char *section, GtkTerm *gtkterm_app, GtkTermWindow *main_window) {

    return g_object_new (GTKTERM_TYPE_TERMINAL, "section", section, "app", gtkterm_app, "main_window", main_window, NULL);
}

static void gtkterm_terminal_constructed (GObject *object) {
    GtkTermTerminal *self = GTKTERM_TERMINAL(object);
    GtkTermTerminalPrivate *priv = gtkterm_terminal_get_instance_private (self);

	//! Load the configuration from [Section] into the port and terminal config
    //! Take [section] as input, term/port conf are the pointers to the return values;
    g_signal_emit(priv->app->config, gtkterm_signals[SIGNAL_GTKTERM_CONFIG_TERMINAL], 0, priv->section, &priv->term_conf);
    g_signal_emit(priv->app->config, gtkterm_signals[SIGNAL_GTKTERM_CONFIG_SERIAL], 0, priv->section, &priv->port_conf);

    priv->serial_port = gtkterm_serial_port_new (priv->port_conf);

  	//!* set terminal properties
    //! TODO: make configurable from the config file
	vte_terminal_set_scroll_on_output(VTE_TERMINAL(self), FALSE);
	vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(self), TRUE);
	vte_terminal_set_mouse_autohide(VTE_TERMINAL(self), TRUE);
	vte_terminal_set_backspace_binding(VTE_TERMINAL(self), VTE_ERASE_ASCII_BACKSPACE);

    vte_terminal_set_cursor_shape (VTE_TERMINAL(self), priv->term_conf->block_cursor ? VTE_CURSOR_SHAPE_BLOCK : VTE_CURSOR_SHAPE_IBEAM);
    vte_terminal_set_font (VTE_TERMINAL (self), priv->term_conf->font);	
    vte_terminal_set_scrollback_lines (VTE_TERMINAL (self), priv->term_conf->scrollback);
    vte_terminal_set_audible_bell (VTE_TERMINAL (self), priv->term_conf->visual_bell);
    vte_terminal_set_color_background (VTE_TERMINAL (self), &priv->term_conf->background_color);
    vte_terminal_set_color_foreground (VTE_TERMINAL (self), &priv->term_conf->foreground_color);	

	//! Update the statusbar and main window title
    g_signal_emit (priv->main_window, gtkterm_signals[SIGNAL_GTKTERM_TERMINAL_CHANGED], 
                                      0, 
                                      priv->section, 
                                      gtkterm_serial_port_get_string (priv->serial_port), 
                                      gtkterm_serial_port_status (priv->serial_port));    

    G_OBJECT_CLASS (gtkterm_terminal_parent_class)->constructed (object);
}

static void gtkterm_terminal_dispose (GObject *object) {
    GtkTermTerminal *self = GTKTERM_TERMINAL(object);
    GtkTermTerminalPrivate *priv = gtkterm_terminal_get_instance_private (self);	
	
    g_free(priv->section);
    g_free(priv->term_conf);
    g_free(priv->port_conf);
}

static void gtkterm_terminal_set_property (GObject *object,
                             unsigned int prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GtkTermTerminal *self = GTKTERM_TERMINAL(object);
    GtkTermTerminalPrivate *priv = gtkterm_terminal_get_instance_private (self);

    switch (prop_id) {
    	case PROP_SECTION:
        	priv->section = g_value_dup_string (value);
        	break;

    	case PROP_GTKTERM_APP:
        	priv->app = g_value_dup_object (value); 
        	break;

    	case PROP_MAIN_WINDOW:
        	priv->main_window = g_value_dup_object (value); 
        	break;			


    	default:
        	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void gtkterm_terminal_class_init (GtkTermTerminalClass *class) {

  	GObjectClass *object_class = G_OBJECT_CLASS (class);
    object_class->set_property = gtkterm_terminal_set_property;
    object_class->constructed = gtkterm_terminal_constructed;
  	object_class->dispose = gtkterm_terminal_dispose;	

	//! Parameters to hand over at creation of the object
	//! We need the section to load the config from the keyfile.
  	gtkterm_terminal_properties[PROP_SECTION] = g_param_spec_string (
        "section",
        "section",
        "section",
        NULL,
        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  	gtkterm_terminal_properties[PROP_GTKTERM_APP] = g_param_spec_object (
        "app",
        "app",
        "app",
        GTKTERM_TYPE_APP,
        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  	gtkterm_terminal_properties[PROP_MAIN_WINDOW] = g_param_spec_object (
        "main_window",
        "main_window",
        "main_window",
        GTKTERM_TYPE_GTKTERM_WINDOW,
        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);		        

    g_object_class_install_properties (object_class, N_PROPS, gtkterm_terminal_properties);
}

static void gtkterm_terminal_init (GtkTermTerminal *self) {

	//! TODO: Make GObject
  //! create_buffer();
}
