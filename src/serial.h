/***********************************************************************/
/* serie.c                                                             */
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

#ifndef SERIAL_H_
#define SERIAL_H_

#ifndef NO_TERMIOS
#include <termios.h>
#endif

extern int serial_port_fd;

int Send_chars(char *, int);
gboolean Config_port(void);
void Set_signals(guint);
int lis_sig(void);
void Close_port(void);
void configure_echo(gboolean);
void configure_crlfauto(gboolean);
void configure_autoreconnect_enable(gboolean);
void configure_esc_clear_screen(gboolean);
void sendbreak(void);
unsigned int set_port_baudrate(unsigned int, int);
gchar* get_port_string(void);

struct baudrate {
	unsigned int baud;
	speed_t speed;
};
extern const struct baudrate baudrate_list[];
extern const int baudrate_count;
extern const gboolean speed_t_is_sane;
speed_t find_standard_baudrate(unsigned int);
unsigned int speed_t_to_baud(speed_t);

#define BUFFER_RECEPTION 8192
#define BUFFER_EMISSION 4096
#define LINE_FEED 0x0A
#define POLL_DELAY 100               /* in ms (for control signals) */

#endif
