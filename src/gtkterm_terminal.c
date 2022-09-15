/************************************************************************/
/* gtkterm_terminal.c                                                   */
/* ------------------                                                   */
/*           GTKTerm Software                                           */
/*                      (c) Julien Schmitt                              */
/*                                                                      */
/* -------------------------------------------------------------------  */
/*                                                                      */
/*   Purpose                                                            */
/*      Handles all VTE in/output to/from serial port                   */
/*                                                                      */
/*   ChangeLog                                                          */
/*       - 2.0   : Port to Gtk4                                         */
/*       - 1.01  : The put_hexadecimal partly function rewritten.       */
/*                 The vte_terminal_get_cursor_position function does   */
/*                 not return always the actual column.                 */
/*                 Now it uses an internal column-index (virt_col_pos). */
/*                 (Willem van den Akker)                               */
/*      - 0.99.7 : Changed keyboard shortcuts to <ctrl><shift>          */
/*	               (Ken Peek)                                           */
/*      - 0.99.6 : Added scrollbar and copy/paste (Zach Davis)          */
/*      - 0.99.5 : Make package buildable on pure *BSD by changing the  */
/*                 include to asm/termios.h by sys/ttycom.h             */
/*                 Print message without converting it into the locale  */
/*                 in show_message()                                    */
/*                 Set backspace key binding to backspace so that the   */
/*                 backspace works. It would even be nicer if the       */
/*                 behaviour of this key could be configured !          */
/*      - 0.99.4 : - Sebastien Bacher -                                 */
/*                 Added functions for CR LF auto mode                  */
/*                 Fixed put_text() to have \r\n for the VTE Widget     */
/*                 Rewritten put_hexadecimal() function                 */
/*                 - Julien -                                           */
/*                 Modified send_serial to return the actual number of  */
/*                 bytes written, and also only display exactly what    */
/*                 is written                                           */
/*      - 0.99.3 : Modified to use a VTE terminal                       */
/*      - 0.99.2 : Internationalization                                 */
/*      - 0.99.0 : \b byte now handled correctly by the ascii widget    */
/*                 SUPPR (0x7F) also prints correctly                   */
/*                 adapted for macros                                   */
/*                 modified "about" dialog                              */
/*      - 0.98.6 : fixed possible buffer overrun in hex send            */
/*                 new "Send break" option                              */
/*      - 0.98.5 : icons in the menu                                    */
/*                 bug fixed with local echo and hexadecimal            */
/*                 modified hexadecimal send separator, and bug fixed   */
/*      - 0.98.4 : new hexadecimal display / send                       */
/*      - 0.98.3 : put_text() modified to fit with 0x0D 0x0A            */
/*      - 0.98.2 : added local echo by Julien                           */
/*      - 0.98 : file creation by Julien                                */
/*                                                                      */
/* This GtkTerm is free software: you can redistribute it and/or modify	*/ 
/* it under the terms of the GNU  General Public License as published  	*/
/* by the Free Software Foundation, either version 3 of the License,   	*/
/* or (at your option) any later version.							   	*/
/*																	   	*/
/* GtkTerm is distributed in the hope that it will be useful, but	   	*/
/* WITHOUT ANY WARRANTY; without even the implied warranty of 		   	*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 			   	*/
/* See the GNU General Public License for more details.					*/
/*																		*/
/* You should have received a copy of the GNU General Public License 	*/
/* along with GtkTerm If not, see <https://www.gnu.org/licenses/>. 		*/
/*                                                                     	*/
/************************************************************************/

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

#include "gtkterm_defaults.h"
#include "gtkterm_window.h"
#include "gtkterm_serial_port.h"
#include "gtkterm_terminal.h"
#include "gtkterm_buffer.h"
#include "macros.h"
#include "gtkterm_configuration.h"

typedef struct  {
    GtkTermBuffer *term_buffer;     /**< Terminal buffer for serial port and terminal                       */
    GtkTermSerialPort *serial_port; /**< The active serial port for this terminal                           */
    term_config_t *term_conf;       /**< The configuration loaded from the keyfile                          */
    port_config_t *port_conf;       /**< Port configuration used in this terminal                           */
    macro_t       *macros;          /**< \todo convert macros -> object                                     */
    GActionGroup *action_group;     /**< Terminal action group                                              */
 
    char *section;           		/**< Section used in this terminal for configuration from config file   */
	GtkTerm *app;                   /**< Pointer to the app for getting [section] and keyfile               */
    GtkTermWindow *main_window;     /**< Pointer to the main window for updating the statusbar on changes   */

} GtkTermTerminalPrivate;

struct _GtkTermTerminal {

    VteTerminal vte_object;         /**< The actual terminal object                                         */
};

struct _GtkTermTerminalClass {

    VteTerminalClass vte_class; 
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


void gtkterm_terminal_view_ascii (GtkTermTerminal *, char *, uint);
void gtkterm_terminal_view_hex (GtkTermTerminal *, char *, uint);

/**
 * @brief Create a new terminal object.
 * 
 * This also binds the parameter to the properties of the terminal.
 * 
 * @param section The section for the configuration in this terminal
 * 
 * @param gtkterm_app The GTKTerm application
 * 
 * @param main_window The main_window this terminal is attached to.
 * 
 * @return The terminal object.
 * 
 */
GtkTermTerminal *gtkterm_terminal_new (char *section, GtkTerm *gtkterm_app, GtkTermWindow *main_window) {

    return g_object_new (GTKTERM_TYPE_TERMINAL, "section", section, "app", gtkterm_app, "main_window", main_window, NULL);
}

/**
 * @brief Callback when new data from the VTE widget is received.
 * 
 * If echo is enabled the string is also send to the buffer. Which will update
 * the terminal by sending the new data signal.
 * 
 * @param widget The VTE widget.
 * 
 * @param text The text which is entered
 * 
 * @param length The length of the text
 * 
 * @param ptr Not used.
 * 
 */
static void gtkterm_terminal_vte_data_received (VteTerminal *widget, char *text, unsigned int length, gpointer ptr) {
    GtkTermTerminal *self = GTKTERM_TERMINAL(ptr);
    GtkTermTerminalPrivate *priv = gtkterm_terminal_get_instance_private (self);
    int chars_transmitted = 0; 

    g_signal_emit(priv->serial_port, gtkterm_signals[SIGNAL_GTKTERM_SERIAL_DATA_TRANSMIT], 0, text, length, &chars_transmitted);

    /** On echo send data to the buffer which will update the terminal */
    if (chars_transmitted > 0 && priv->term_conf->echo) {
        /** Convert to GBytes, needed for the buffer */
        GBytes *data = g_bytes_new (text, chars_transmitted);
	    
        /** Send signal to buffer when new data is arrived */
        g_signal_emit (self, gtkterm_signals[SIGNAL_GTKTERM_VTE_DATA_RECEIVED], 0, data);
    }
}

/**
 * @brief When signalsof the serial port is changed we get a signal and have to update GtkTermWindow.
 * 
 * @param object The serial port.
 * 
 * @param pspec Not used.
 * 
 * @param user_data The active terminal.
 * 
 */
static void gtkterm_terminal_port_signals_changed (GObject *object, GParamSpec *pspec, gpointer user_data) {
    GtkTermTerminal *self = GTKTERM_TERMINAL(user_data);
    GtkTermTerminalPrivate *priv = gtkterm_terminal_get_instance_private (self);

    g_signal_emit (priv->main_window, gtkterm_signals[SIGNAL_GTKTERM_SERIAL_SIGNALS_CHANGED], 0, 
                   gtkterm_serial_port_get_signals (GTKTERM_SERIAL_PORT (object)));  
}

/**
 * @brief When the status of the serial port is changed we get a signal and have to update GtkTermWindow.
 * 
 * @param object The serial port.
 * 
 * @param pspec Not used.
 * 
 * @param user_data The active terminal.
 * 
 */
static void gtkterm_terminal_port_status_changed (GObject *object, GParamSpec *pspec, gpointer user_data) {
    char *serial_string;
    GError *error;
    GtkTermTerminal *self = GTKTERM_TERMINAL(user_data);
    GtkTermTerminalPrivate *priv = gtkterm_terminal_get_instance_private (self);
    GtkTermSerialPortState status = gtkterm_serial_port_get_status (GTKTERM_SERIAL_PORT (object));

    if (status == GTKTERM_SERIAL_PORT_ERROR) {
        char *error_string;
        error = gtkterm_serial_port_get_error (GTKTERM_SERIAL_PORT (object));

        if (error != NULL) 
            error_string = g_strdup_printf (_("Serial port error : %s"), error->message);
        else
            error_string = g_strdup (_("Serial port error : Unknown error"));

        /** \todo convert to notify signal on message... */
        gtkterm_show_infobar (priv->main_window, error_string, GTK_MESSAGE_ERROR); 

        g_free(error_string);
    }

	/** Update the statusbar and main window title */
    serial_string = gtkterm_serial_port_get_string (priv->serial_port);
    g_signal_emit (priv->main_window, gtkterm_signals[SIGNAL_GTKTERM_TERMINAL_CHANGED], 
                                      0, 
                                      priv->section, 
                                      serial_string, 
                                      GtkTermSerialPortStateString[status]);    
    g_free(serial_string);
}

/**
 * @brief When the buffer is updated the terminal is notified data new data is available.
 * 
 * Depending of the setting the output will be in ASCII for HEX.
 * 
 * @param object Not used.
 * 
 * @param data The new string of data in the buffer.
 * 
 * @param length The length of the new string
 * 
 * @param user_data The terminal.
 * 
 */
 static void gtkterm_terminal_buffer_updated (GObject *object, gpointer data, unsigned int length, gpointer user_data) {
     GtkTermTerminal *self = GTKTERM_TERMINAL(user_data);
     GtkTermTerminalPrivate *priv = gtkterm_terminal_get_instance_private (self);

    if (priv->term_conf->view_mode == GTKTERM_TERMINAL_VIEW_ASCII) 
        gtkterm_terminal_view_ascii (self, data, length);
    else
        gtkterm_terminal_view_hex (self, data, length);
}

/**
 * @brief Constructs the terminal.
 * 
 * Setup signals, initialize terminal etc.
 * 
 * @param object The terminal object we are constructing.
 * 
 */
static void gtkterm_terminal_constructed (GObject *object) {
    GError *error;
    GtkTermTerminal *self = GTKTERM_TERMINAL(object);
    GtkTermTerminalPrivate *priv = gtkterm_terminal_get_instance_private (self);

    /** Check if the config file exists, if not it will be created */
    g_signal_emit(priv->app->config, gtkterm_signals[SIGNAL_GTKTERM_CONFIG_CHECK_FILE], 0);

    if ((gtkterm_configuration_get_status(priv->app->config)) != GTKTERM_CONFIGURATION_SUCCESS) {
        error = gtkterm_configuration_get_error (priv->app->config);
        /** \todo: convert to notify on message */        
	    gtkterm_show_infobar (priv->main_window, error->message, GTK_MESSAGE_INFO); 
    }

	/** 
     * Load the configuration from [Section] into the port and terminal config.
     * Take [section] as input, term/port conf are the pointers to the return values.
     */
    g_signal_emit(priv->app->config, gtkterm_signals[SIGNAL_GTKTERM_CONFIG_TERMINAL], 0, priv->section, &priv->term_conf);
    g_signal_emit(priv->app->config, gtkterm_signals[SIGNAL_GTKTERM_CONFIG_SERIAL], 0, priv->section, &priv->port_conf);
    
    /** 
     * Create the serial port. The buffer will be a propertie, so they can exchange data without
     * interference of the terminal.
     */
    priv->serial_port = gtkterm_serial_port_new (priv->port_conf);

    /** Create a buffer for the terminal */
    priv->term_buffer = gtkterm_buffer_new (priv->serial_port, self, priv->term_conf);

    g_signal_connect (G_OBJECT(priv->serial_port), "notify::port-status", G_CALLBACK(gtkterm_terminal_port_status_changed), self);
    g_signal_connect (G_OBJECT(priv->serial_port), "notify::port-signals", G_CALLBACK(gtkterm_terminal_port_signals_changed), self);
    g_signal_connect (G_OBJECT(priv->term_buffer), "buffer-updated", G_CALLBACK(gtkterm_terminal_buffer_updated), self);    

    /** Send initial notify to update the status bar */
	g_object_notify(G_OBJECT(priv->serial_port), "port-status");		

  	/**  
     * Set terminal properties.
     * \todo: make configurable from the config file
     */
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
    vte_terminal_set_size(VTE_TERMINAL (self), priv->term_conf->columns, priv->term_conf->rows);	

    G_OBJECT_CLASS (gtkterm_terminal_parent_class)->constructed (object);
}

/**
 * @brief Called when distroying the terminal
 * 
 * This is used to clean up an freeing the variables in the terminal structure.
 * 
 * @param object The object.
 * 
 */
static void gtkterm_terminal_dispose (GObject *object) {
    GtkTermTerminal *self = GTKTERM_TERMINAL(object);
    GtkTermTerminalPrivate *priv = gtkterm_terminal_get_instance_private (self);	
	
    g_free(priv->section);
    g_free(priv->term_conf);
    g_free(priv->port_conf);
}

/**
 * @brief Set the property of the GtkTermTerminal structure
 * 
 * This is used to initialize the variables when creating a new terminal
 * 
 * @param object The object.
 * 
 * @param prop_id The id of the property to set.
 * 
 * @param value The value for the property
 * 
 * @param pspec Metadata for property setting.
 * 
 */
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

/**
 * @brief Initializing the terminal class
 * 
 * Setting the properties and callback functions
 * 
 * @param class The terminal class
 * 
 */
static void gtkterm_terminal_class_init (GtkTermTerminalClass *class) {

  	GObjectClass *object_class = G_OBJECT_CLASS (class);
    object_class->set_property = gtkterm_terminal_set_property;
    object_class->constructed = gtkterm_terminal_constructed;
  	object_class->dispose = gtkterm_terminal_dispose;	

    /** Connect the serial port */
  	gtkterm_signals[SIGNAL_GTKTERM_SERIAL_CONNECT] = g_signal_new ("serial-connect",
                                                GTKTERM_TYPE_SERIAL_PORT,
                                                G_SIGNAL_RUN_FIRST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_NONE,
                                                0,
                                                NULL); 

    /** We received data from the VTE widget and send it to the serial port */
  	gtkterm_signals[SIGNAL_GTKTERM_SERIAL_DATA_TRANSMIT] = g_signal_new ("serial-data-transmit",
                                                GTKTERM_TYPE_SERIAL_PORT,
                                                G_SIGNAL_RUN_FIRST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_INT,
                                                2,
                                                G_TYPE_POINTER,
                                                G_TYPE_INT,
                                                NULL);                                                 

    gtkterm_signals[SIGNAL_GTKTERM_VTE_DATA_RECEIVED] = g_signal_new ("vte-data-received",
                                                   GTKTERM_TYPE_TERMINAL,
                                                   G_SIGNAL_RUN_FIRST,
                                                   0,
                                                   NULL,
                                                   NULL,
                                                   NULL,
                                                   G_TYPE_NONE,
                                                   1,
                                                   G_TYPE_BYTES,
                                                   0);

	/** 
     * Parameters to hand over at creation of the object
	 * We need the section to load the config from the keyfile.
     */
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

/**
 * @brief Initialize the terminal with the actions, etc.
 * 
 * @param self The terminal we are initializing.
 * 
 */
static void gtkterm_terminal_init (GtkTermTerminal *self) {
    GtkTermTerminalPrivate *priv = gtkterm_terminal_get_instance_private (self);

    /** Set the menu actions for this terminal */

    g_signal_connect_after (G_OBJECT (self), "commit", G_CALLBACK (gtkterm_terminal_vte_data_received), self);

}

/**
 * @brief Outputs a string to the terminal widget in ASCII.
 * 
 * @param self The terminal.
 * 
 * @param data The string of data we want to show
 * 
 * @param length The length of the string
 * 
 */
void gtkterm_terminal_view_ascii (GtkTermTerminal *self, char *data, unsigned int length) {

    vte_terminal_feed (VTE_TERMINAL(self), data, length);
}

/**
 * @brief Outputs a string to the terminal widget in HEX layout.
 * 
 * @param self The terminal.
 * 
 * @param data The string of data we want to show
 * 
 * @param length The length of the string
 * 
 */
void gtkterm_terminal_view_hex (GtkTermTerminal *self, char *data, unsigned int length) {

}
