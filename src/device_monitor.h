/***********************************************************************/
/* device_mintor.h                                                     */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Monitor device to autoreconnect                                */
/*   Written by Kevin Picot - picotk27@gmail.com                       */
/*                                                                     */
/***********************************************************************/

#ifndef DEV_MON_H_
#define DEV_MON_H_

#include <stdbool.h>

extern void device_monitor_start(void);
extern void device_autoreconnect_enable(bool enabled);

#endif
