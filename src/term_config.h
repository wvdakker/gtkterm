/***********************************************************************/
/* config.h                                                            */
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

#include <gtk/gtk.h>

void ConfigFlags(void);
void check_text_input(GtkEditable *editable,
                      gchar       *new_text,
                      gint         new_text_length,
                      gint        *position,
                      gpointer     user_data);

struct configuration_port
{
	gchar port[1024];
	guint vitesse;               // baud rate
	gint bits;                   // 5 - 6 - 7 - 8
	gint stops;                  // 1 - 2
	gint parite;                 // 0 : None, 1 : Odd, 2 : Even
	gint flux;                   // 0 : None, 1 : Xon/Xoff, 2 : RTS/CTS, 3 : RS485halfduplex
	gint delai;                  // end of char delay: in ms
	gint rs485_rts_time_before_transmit;
	gint rs485_rts_time_after_transmit;
	gchar car;                   // caractere attendre
	gboolean echo;               // echo local
	gboolean crlfauto;           // line feed auto
	gboolean autoreconnect_enabled;	// enable autoreconnect
	gboolean esc_clear_screen;   // clear screen when receive ESC char ('\x1b' - 27)
	gboolean timestamp;
	gboolean disable_port_lock;
	gboolean disable_hotkeys;
};

typedef struct
{
	gboolean block_cursor;
	gint rows;
	gint columns;
	gint scrollback;
	gboolean visual_bell;
	GdkRGBA foreground_color;
	GdkRGBA background_color;
	PangoFontDescription *font_desc;
} display_config_t;


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

#endif
