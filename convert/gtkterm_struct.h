/************************************************************************/
/* gtkterm_struct.c                                                     */
/* ----------------                                                     */
/*           GTKTerm Software                                           */
/*                      (c) Julien Schmitt                              */
/*                                                                      */
/* -------------------------------------------------------------------  */
/*                                                                      */
/*   Purpose                                                            */
/*      Defines uses structures				                            */
/*                                                                      */
/*   ChangeLog                                                          */
/*      - 2.0 : Initial  file creation                                  */
/*                                                                      */
/* This GtkTerm is free software: you can redistribute it and/or modify	*/ 
/* it under the terms of the GNU  General Public License as published  	*/
/* by the Free Software Foundation, either version 3 of the License,   	*/
/* or (at your option) any later version.							   	*/
/*													                 	*/
/* GtkTerm is distributed in the hope that it will be useful, but	   	*/
/* WITHOUT ANY WARRANTY; without even the implied warranty of 		   	*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 			   	*/
/* See the GNU General Public License for more details.					*/
/*																	    */
/* You should have received a copy of the GNU General Public License 	*/
/* along with GtkTerm If not, see <https://www.gnu.org/licenses/>. 		*/
/*                                                                     	*/
/************************************************************************/


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


/**
 * @brief The typedef for mode in which the output of the terminal can be viewed.
 *
 */
typedef enum  {
    GTKTERM_TERMINAL_VIEW_ASCII,
    GGTKTERM_TERMINAL_VIEW_HEX

} GtkTermTerminalView;


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
	char char_queue;            		// character in queue
	bool echo;               			// echo local
	bool auto_cr;           			// line feed auto
	bool auto_lf;           			// return auto
	bool timestamp;
	int delay;                  		// end of char delay: in ms
	int rows;
	int columns;
	int scrollback;
	int show_index;						/** Show index in output		*/
	GtkTermTerminalView view_mode;		/** Mode to view output in		*/
	int hex_chars;						/** Number of chars in hex mode	*/	
	bool visual_bell;
	GdkRGBA foreground_color;
	GdkRGBA background_color;
	PangoFontDescription *font;
	char *active_section;

	char *default_filename;

} term_config_t;

extern term_config_t term_conf;
extern port_config_t port_conf;
