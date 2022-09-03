/************************************************************************/
/* gtkterm_defaults.h                                                 	*/
/* ------------------                                                	*/
/*           GTKTerm Software                                          	*/
/*                      (c) Julien Schmitt                             	*/
/*                                                                     	*/
/* ------------------------------------------------------------------- 	*/
/*                                                                     	*/
/*   Purpose                                                           	*/
/*      Include file for the default used in gtkterm                   	*/
/*                                                                     	*/
/* This GtkTerm is free software: you can redistribute it and/or modify	*/ 
/* it under the terms of the GNU  General Public License as published  	*/
/* by the Free Software Foundation, either version 3 of the License,   	*/
/* or (at your option) any later version.							   	*/
/*																	   	*/
/* GtkTerm is distributed in the hope that it will be useful, but	   	*/
/* WITHOUT ANY WARRANTY; without even the implied warranty of 		   	*/
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 			   	*/
/* See the GNU General Public License for more details.					*/
/*																		*/
/* You should have received a copy of the GNU General Public License 	*/
/* along with GtkTerm If not, see <https://www.gnu.org/licenses/>. 		*/
/*                                                                     	*/
/************************************************************************/

#ifndef GTKTERM_DEFAULTS_H
#define GTKTERM_DEFAULTS_H

/** Defaults for VTE-terminal    */
#define DEFAULT_FONT            "Monospace 12"
#define DEFAULT_SCROLLBACK      10000
#define DEFAULT_DELAY 		    0
#define DEFAULT_CHAR 		    -1
#define DEFAULT_DELAY_RS485     30
#define DEFAULT_ECHO 		    "false"
#define DEFAULT_VISUAL_BELL     "false"

/** Defaults for serial ports    */
#define DEFAULT_PORT 	        "/dev/ttyS0"
#define DEFAULT_BAUDRATE        115200
#define DEFAULT_PARITY 	        "none"
#define DEFAULT_BITS 	        8
#define DEFAULT_STOPBITS        1
#define DEFAULT_FLOW 	        "none"

/** 
 * The buffers for receive and transmit are internal buffers for the communication API.
 * It is not the buffersize for the terminal/serialport communication
 */
#define GTKTERM_SERIAL_PORT_RECEIVE_BUFFER_SIZE  8192   /**< Size of the receive buffer for the serial port     */
#define GTKTERM_SERIAL_PORT_TRANSMIT_BUFFER_SIZE 4096   /**< Size of the transmit buuffer for the serial port   */

#define LINE_FEED               0x0A
#define POLL_DELAY              100                     /**< in ms (for control signals)                        */

/** Generic defaults            */
#define BUFFER_LENGTH           256
#define MAX_SECTION_LENGTH      32
#define GTKTERM_MESSAGE_LENGTH  128
#define DEFAULT_SECTION		   "default"		        /**< Default section if not specified	                */
#define CONFIGURATION_FILENAME ".gtktermrc"		        /**< Name of the resource file                          */
#define CONF_ITEM_LENGTH		32
#define DEFAULT_STRING_LEN      32

/** Type of terminal view       */
#define ASCII_VIEW              0
#define HEXADECIMAL_VIEW        1

#define BUFFER_SIZE             (128 * 1024)            /**< Size of the buffer between the terminal and port   */

#endif // GTKTERM_DEFAULTS_H