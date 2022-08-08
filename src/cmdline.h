/***********************************************************************/
/* cmdline.h                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Reads the command line                                         */
/*      - Header file -                                                */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 2.0 : migrated to GTK4                                       */
/*      - 0.98 : file creation by Julien                               */
/*                                                                     */
/***********************************************************************/extern GOptionGroup *g_term_group;

#ifndef CMDLINE_H
#define CMDLINE_H

void gtkterm_add_cmdline_options (GtkTerm *app);

#endif // CMDLINE_H