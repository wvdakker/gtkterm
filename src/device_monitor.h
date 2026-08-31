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

void device_monitor_start(void);
void device_monitor_stop(void);
void device_monitor_clear_resume_reconnect(void);

#endif
