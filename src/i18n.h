/***********************************************************************/
/* i18n.h                                                              */
/* ------                                                              */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      UTF-8 conversion to print in the console                       */
/*        - Header file -                                              */
/*                                                                     */
/***********************************************************************/

#ifndef I18N_H_
#define I18N_H

#include <stdio.h>

int i18n_printf(const char *, ...);
int i18n_fprintf(FILE *, const char *, ...);
void i18n_perror(const char *);
char *strerror_utf8(int);

#endif
