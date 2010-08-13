/***********************************************************************/
/* gtkterm.c                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                      julien@jls-info.com                            */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Main program file                                              */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.99.0 : added call to add_shortcuts()                       */
/*      - 0.98 : all GUI functions moved to widgets.c                  */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <stdlib.h>

#include "widgets.h"
#include "serie.h"
#include "config.h"
#include "cmdline.h"
#include "parsecfg.h"
#include "buffer.h"
#include "macros.h"
#include "auto_config.h"
#include "gettext.h"

int main(int argc, char *argv[])
{
  gchar *message;

  config_file = g_strdup_printf("%s/.gtktermrc", getenv("HOME"));

  gtk_set_locale();
  (void)bindtextdomain(PACKAGE, LOCALEDIR);
  (void)bind_textdomain_codeset(PACKAGE, "UTF-8");
  (void)textdomain(PACKAGE);

  gtk_init(&argc, &argv);

  create_buffer();
 
  create_main_window();
  
  if(read_command_line(argc, argv) < 0)
    {
      delete_buffer();
      exit(1);
    }

  message = Config_port();
  if(message == NULL)
    message = g_strdup_printf("No open port");

  Set_Font();
  add_shortcuts();

  Set_status_message(message);

  set_view(ASCII_VIEW);

  gtk_main();

  delete_buffer();

  Close_port_and_remove_lockfile();

  return 0;
}
