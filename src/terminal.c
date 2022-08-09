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
