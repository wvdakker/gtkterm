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

#ifndef SERIAL_H_
#define SERIAL_H_

#define DEFAULT_PORT 	 "/dev/ttyS0"
#define DEFAULT_BAUDRATE 115200
#define DEFAULT_PARITY 	 "none"
#define DEFAULT_BITS 	 8
#define DEFAULT_STOPBITS 1
#define DEFAULT_FLOW 	 "none"

typedef struct
{
	char port[256];
	long int baudrate;              // 300 - 600 - 1200 - ... - 2000000
	int bits;                   	// 5 - 6 - 7 - 8
	int stopbits;                   // 1 - 2
	int parity;                 	// 0 : None, 1 : Odd, 2 : Even
	int flow_control;               // 0 : None, 1 : Xon/Xoff, 2 : RTS/CTS, 3 : RS485halfduplex
	int rs485_rts_time_before_transmit;
	int rs485_rts_time_after_transmit;
	char char_queue;            	// character in queue
	bool disable_port_lock;

} port_config_t;

extern int serial_port_fd;
extern port_config_t port_conf;

char* get_port_string (void);

#define RECEIVE_BUFFER 8192
#define TRANSMIT_BUFFER 4096
#define LINE_FEED 0x0A
#define POLL_DELAY 100               /* in ms (for control signals) */

#endif
