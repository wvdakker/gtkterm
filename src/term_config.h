/***********************************************************************/
/* term_config.h                                                       */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Configuration of the serial port                               */
/*      - Header file -                                                */
/*                                                                     */
/***********************************************************************/

#ifndef TERM_CONFIG_H_
#define TERM_CONFIG_H_

#define DEFAULT_FONT "Monospace 12"
#define DEFAULT_SCROLLBACK 10000

#define DEFAULT_PORT "/dev/ttyS0"
#define DEFAULT_SPEED 115200
#define DEFAULT_PARITY 0
#define DEFAULT_BITS 8
#define DEFAULT_STOP 1
#define DEFAULT_FLOW 0
#define DEFAULT_DELAY 0
#define DEFAULT_CHAR -1
#define DEFAULT_DELAY_RS485 30
#define DEFAULT_ECHO FALSE

struct configuration_port
{
	char port[1024];
	uint32_t speed;                	  // 300 - 600 - 1200 - ... - 2000000
	uint8_t bits;                   // 5 - 6 - 7 - 8
	uint8_t stops;                  // 1 - 2
	uint8_t parity;                 // 0 : None, 1 : Odd, 2 : Even
	uint8_t flux;                   // 0 : None, 1 : Xon/Xoff, 2 : RTS/CTS, 3 : RS485halfduplex
	uint16_t delay;                  // end of char delay: in ms
	uint16_t rs485_rts_time_before_transmit;
	uint16_t rs485_rts_time_after_transmit;
	char char_queue;            // character in queue
	gboolean echo;               // echo local
	gboolean crlfauto;           // line feed auto
	gboolean timestamp;
	gboolean disable_port_lock;
};

typedef struct
{
	gboolean block_cursor;
	int rows;
	int columns;
	int scrollback;
	gboolean visual_bell;
	GdkRGBA foreground_color;
	GdkRGBA background_color;
	char *font;

} display_config_t;

extern struct configuration_port config;

void config_file_init (void);
void verify_configuration (void);
int load_configuration_from_file (char *);
void hard_default_configuration (void);
void copy_configuration (int);
int check_configuration_file(void);

#endif
