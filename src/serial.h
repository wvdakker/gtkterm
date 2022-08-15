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


typedef struct
{
	char *port;
	long int baudrate;              // 300 - 600 - 1200 - ... - 2000000
	int bits;                   	// 5 - 6 - 7 - 8
	int stopbits;                   // 1 - 2
	int parity;                 	// 0 : None, 1 : Odd, 2 : Even
	int flow_control;               // 0 : None, 1 : Xon/Xoff, 2 : RTS/CTS, 3 : RS485halfduplex
	int rs485_rts_time_before_transmit;
	int rs485_rts_time_after_transmit;
	bool disable_port_lock;

} port_config_t;

G_BEGIN_DECLS

typedef struct _GtkTermSerialPort GtkTermSerialPort;

#define GTKTERM_TYPE_SERIAL_PORT gtkterm_serial_port_get_type ()
G_DECLARE_FINAL_TYPE (GtkTermSerialPort, gtkterm_serial_port, GTKTERM, SERIAL_PORT, GObject)

GtkTermSerialPort *gtkterm_serial_port_new (void);

char* gtkterm_serial_port_get_string (GtkTermSerialPort *);

G_END_DECLS

#endif
