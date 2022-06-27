/***********************************************************************/
/* serial.h                                                            */
/* -------                                                             */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Serial port access functions                                   */
/*      - Header file -                                                */
/*                                                                     */
/***********************************************************************/

#ifndef SERIE_H_
#define SERIE_H_

extern int serial_port_fd;

char* get_port_string (void);

#define RECEIVE_BUFFER 8192
#define TRANSMIT_BUFFER 4096
#define LINE_FEED 0x0A
#define POLL_DELAY 100               /* in ms (for control signals) */

#endif
