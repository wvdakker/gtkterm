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

#include "terminal.h"

struct _GtkTermTerminalClass {

    VteTerminalClass vte_class; /// The vte class
};

G_DEFINE_FINAL_TYPE (GtkTermTerminal, gtkterm_terminal, VTE_TYPE_TERMINAL)

static void gtkterm_terminal_class_init (GtkTermTerminalClass *self) {

}

static void gtkterm_terminal_init (GtkTermTerminal *self) {

	self->filename = NULL;

	  //! TODO: Make GObject
  //! create_buffer();

  /* set terminal properties, these could probably be made user configurable */
	//vte_terminal_set_scroll_on_output(VTE_TERMINAL(display), FALSE);
	//vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(display), TRUE);
	//vte_terminal_set_mouse_autohide(VTE_TERMINAL(display), TRUE);
	//vte_terminal_set_backspace_binding(VTE_TERMINAL(display), VTE_ERASE_ASCII_BACKSPACE);
}

//set_color (&term_conf.foreground_color, 0.66, 0.66, 0.66, 1.0);
//set_color (&term_conf.background_color, 0, 0, 0, 1.0);

// //! Convert the colors RGB to internal color scheme
// void set_color(GdkRGBA *color, float R, float G, float B, float A)
// {
// 	color->red = R;
// 	color->green = G;
// 	color->blue = B;
// 	color->alpha = A;
// }
