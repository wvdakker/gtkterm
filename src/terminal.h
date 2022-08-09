/***********************************************************************/
/* terminal.h                                                       */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Handles all VTE in/output to/from serial port                  */
/*      - Header file -                                                */
/*                                                                     */
/***********************************************************************/
#ifndef TERMINAL_H
#define TERMINAL_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

#include <serial.h>

G_BEGIN_DECLS

struct _GtkTermTerminal {
    uint8_t view_mode;              //! ASCII or HEX view mode
 //   GtkTermBuffer *term_buffer;
    display_config_t *term_conf;    //! The configuration loaded from the keyfile
    port_config_t *port_conf;       //! Port configuration used in this terminal
    char *active_section;           //! Active section in this window from config file

    char *filename;                 //! File to send

    GdkRGBA *term_forground;        //! Foreground (text) color of this terminal
    GdkRGBA *term_background;       //! Background color

    VteTerminal vte_object;         //! The actual terminal
};


#define GTKTERM_TERMINAL_TYPE gtkterm_terminal_get_type()
G_DECLARE_FINAL_TYPE (GtkTermTerminal, gtkterm_terminal, GTKTERM, TERMINAL, VteTerminal)

G_END_DECLS

#endif // TERMINAL_H