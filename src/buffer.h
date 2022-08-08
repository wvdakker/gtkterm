/***********************************************************************/
/* buffer.h                                                            */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Management of a local buffer of data received                  */
/*      - Header file -                                                */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.7 : removed auto crlf stuff - (use macros instead)      */
/*      - 0.98.4 : file creation by Julien                             */
/*                                                                     */
/***********************************************************************/

#ifndef BUFFER_H_
#define BUFFER_H_

#define BUFFER_SIZE (128 * 1024)

void create_buffer(void);
void delete_buffer(void);
void put_chars(const char *, unsigned int, bool);
void clear_buffer(void);
void write_buffer(void);
void set_display_func(void (*func)(const char *, uint32_t));
void unset_display_func(void (*func)(const char *, uint32_t));
void set_clear_func(void (*func)(void));
void unset_clear_func(void (*func)(void));
void write_buffer_with_func(void (*func)(const char *, uint32_t));

#endif
