
#define DEFAULT_FONT "Monospace 12"
#define DEFAULT_SCROLLBACK 10000

#define DEFAULT_DELAY 		0
#define DEFAULT_CHAR 		-1
#define DEFAULT_DELAY_RS485 30
#define DEFAULT_ECHO 		"false"
#define DEFAULT_VISUAL_BELL "false"

#define DEFAULT_PORT 	 "/dev/ttyS0"
#define DEFAULT_BAUDRATE 115200
#define DEFAULT_PARITY 	 "none"
#define DEFAULT_BITS 	 8
#define DEFAULT_STOPBITS 1
#define DEFAULT_FLOW 	 "none"

typedef struct
{
	char port[PATH_MAX];
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

typedef struct
{
	bool block_cursor;
	bool show_cursor;
	char char_queue;             // character in queue
	bool echo;               // echo local
	bool crlfauto;           // line feed auto
	bool timestamp;
	int delay;                  // end of char delay: in ms
	int rows;
	int columns;
	int scrollback;
	bool visual_bell;
	GdkRGBA foreground_color;
	GdkRGBA background_color;
	PangoFontDescription *font;
	char *active_section;

	char *default_filename;

} display_config_t;

extern display_config_t term_conf;
extern port_config_t port_conf;
