/***********************************************************************/
/* config.h                                                            */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                      julien@jls-info.com                            */                      
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Configuration of the serial port                               */
/*      - Header file -                                                */
/*                                                                     */
/***********************************************************************/

#ifndef CONFIG_H_
#define CONFIG_H_

gint Config_Port_Fenetre(GtkWidget *widget, guint param);
gint Lis_Config(GtkWidget *bouton, GtkWidget **Combos);
gint Config_Terminal(GtkWidget *widget, guint param);
void Set_Font(void);
gint config_window(gpointer *, guint);
void Verify_configuration(void);
gint Load_configuration_from_file(gchar *);
gint Check_configuration_file(void);

struct configuration_port {
  gchar port[64];
  gint vitesse;                // 300 - 600 - 1200 - ... - 115200
  gint bits;                   // 5 - 6 - 7 - 8
  gint stops;                  // 1 - 2
  gint parite;                 // 0 : aucune, 1 : impaire, 2 : paire
  gint flux;                   // 0 : aucun, 1 : Xon/Xoff, 2 : RTS/CTS
  gint delai;                  // delai de fin de ligne : en ms
  gchar car;             // caractere à attendre
  gboolean echo;               // echo local
  gboolean crlfauto;           // line feed auto
};

typedef struct {
  gboolean transparency;
  gboolean show_cursor;
  gint rows;
  gint columns;
  gint scrollback;
  gboolean visual_bell;
  GdkColor foreground_color;
  GdkColor background_color;
  gdouble background_saturation;
  gchar *font;
} display_config_t;


#define DEFAULT_FONT "Nimbus Mono L, 14"

#define DEFAULT_PORT "/dev/ttyS0"
#define DEFAULT_SPEED 9600
#define DEFAULT_PARITY 0
#define DEFAULT_BITS 8
#define DEFAULT_STOP 1
#define DEFAULT_FLOW 0
#define DEFAULT_DELAY 0
#define DEFAULT_CHAR -1
#define DEFAULT_ECHO FALSE

extern gchar *config_file;

#endif
