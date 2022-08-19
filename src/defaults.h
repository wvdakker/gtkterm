//! Default for VTE-terminal
#define DEFAULT_FONT            "Monospace 12"
#define DEFAULT_SCROLLBACK      10000
#define DEFAULT_DELAY 		    0
#define DEFAULT_CHAR 		    -1
#define DEFAULT_DELAY_RS485     30
#define DEFAULT_ECHO 		    "false"
#define DEFAULT_VISUAL_BELL     "false"

//! Default for serial ports
#define DEFAULT_PORT 	        "/dev/ttyS0"
#define DEFAULT_BAUDRATE        115200
#define DEFAULT_PARITY 	        "none"
#define DEFAULT_BITS 	        8
#define DEFAULT_STOPBITS        1
#define DEFAULT_FLOW 	        "none"

#define RECEIVE_BUFFER          8192
#define TRANSMIT_BUFFER         4096
#define LINE_FEED               0x0A
#define POLL_DELAY              100               //!< in ms (for control signals) 

//! Generic defaults
#define BUFFER_LENGTH           256
#define MAX_SECTION_LENGTH      32