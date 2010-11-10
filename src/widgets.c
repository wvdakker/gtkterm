/***********************************************************************/
/* widgets.c                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Functions for the management of the GUI for the main window    */
/*                                                                     */
/*   ChangeLog                                                         */
/*   (All changes by Julien Schmitt except when explicitly written)    */
/*                                                                     */
/*      - 0.99.6 : Added scrollbar and copy/paste (Zach Davis)         */
/*                                                                     */
/*      - 0.99.5 : Make package buildable on pure *BSD by changing the */
/*                 include to asm/termios.h by sys/ttycom.h            */
/*                 Print message without converting it into the locale */
/*                 in show_message()                                   */
/*                 Set backspace key binding to backspace so that the  */
/*                 backspace works. It would even be nicer if the      */
/*                 behaviour of this key could be configured !         */
/*      - 0.99.4 : - Sebastien Bacher -                                */
/*                 Added functions for CR LF auto mode                 */
/*                 Fixed put_text() to have \r\n for the VTE Widget    */
/*                 Rewritten put_hexadecimal() function                */
/*                 - Julien -                                          */
/*                 Modified send_serial to return the actual number of */
/*                 bytes written, and also only display exactly what   */
/*                 is written                                          */
/*      - 0.99.3 : Modified to use a VTE terminal                      */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.99.0 : \b byte now handled correctly by the ascii widget   */
/*                 SUPPR (0x7F) also prints correctly                  */
/*                 adapted for macros                                  */
/*                 modified "about" dialog                             */
/*      - 0.98.6 : fixed possible buffer overrun in hex send           */
/*                 new "Send break" option                             */
/*      - 0.98.5 : icons in the menu                                   */
/*                 bug fixed with local echo and hexadecimal           */
/*                 modified hexadecimal send separator, and bug fixed  */
/*      - 0.98.4 : new hexadecimal display / send                      */
/*      - 0.98.3 : put_text() modified to fit with 0x0D 0x0A           */
/*      - 0.98.2 : added local echo by Julien                          */
/*      - 0.98 : file creation by Julien                               */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#if defined (__linux__)
#  include <asm/termios.h>       /* For control signals */
#endif
#if defined (__FreeBSD__) || defined (__FreeBSD_kernel__) \
     || defined (__NetBSD__) || defined (__NetBSD_kernel__) \
     || defined (__OpenBSD__) || defined (__OpenBSD_kernel__)
#  include <sys/ttycom.h>        /* For control signals */
#endif
#include <vte/vte.h>
#include <stdio.h>
#include <string.h>

#include "term_config.h"
#include "fichier.h"
#include "serie.h"
#include "widgets.h"
#include "buffer.h"
#include "macros.h"
#include "auto_config.h"

#include <config.h>
#include <glib/gi18n.h>

guint id;
gboolean echo_on;
GtkWidget *StatusBar;
GtkWidget *signals[6];
static GtkWidget *echo_menu = NULL;
static GtkWidget *ascii_menu = NULL;
static GtkWidget *hex_menu = NULL;
static GtkWidget *hex_len_menu = NULL;
static GtkWidget *hex_chars_menu = NULL;
static GtkWidget *show_index_menu = NULL;
static GtkWidget *Hex_Box;
GtkWidget *scrolled_window;
GtkWidget *Fenetre;
GtkAccelGroup *shortcuts;
GtkWidget *display = NULL;

GtkWidget *Text;
GtkTextBuffer *buffer;
GtkTextIter iter;

/* Variables for hexadecimal display */
static gint bytes_per_line = 16;
static gchar blank_data[128];
static guint total_bytes;
static gboolean show_index = FALSE;

/* Local functions prototype */
gint signaux(GtkWidget *, guint);
gint a_propos(GtkWidget *, guint);
gboolean Envoie_car(GtkWidget *, GdkEventKey *, gpointer);
gboolean control_signals_read(void);
gint Toggle_Echo(gpointer *, guint, GtkWidget *);
gint view(gpointer *, guint, GtkWidget *);
gint hexadecimal_chars_to_display(gpointer *, guint, GtkWidget *);
gint toggle_index(gpointer *, guint, GtkWidget *);
gint show_hide_hex(gpointer *, guint, GtkWidget *);
void initialize_hexadecimal_display(void);
gboolean Send_Hexadecimal(GtkWidget *, GdkEventKey *, gpointer);
void Put_temp_message(const gchar *, gint);
gboolean pop_message(void);
static gchar *translate_menu(const gchar *, gpointer);
static void Got_Input(VteTerminal *, gchar *, guint, gpointer);

gint gui_paste(void);
gint gui_copy(void);
gint gui_copy_all_clipboard(void);


/* Menu */
#define NUMBER_OF_ITEMS 36

static GtkItemFactoryEntry Tableau_Menu[] = {
  {N_("/_File") , NULL, NULL, 0, "<Branch>"},
  {N_("/File/Clear screen") , "<ctrl>L", (GtkItemFactoryCallback)clear_buffer, 0, "<StockItem>", GTK_STOCK_CLEAR},
  {N_("/File/Send _raw file") , "<ctrl>R", (GtkItemFactoryCallback)fichier, 1, "<StockItem>",GTK_STOCK_JUMP_TO},
  {N_("/File/_Save raw file") , NULL, (GtkItemFactoryCallback)fichier, 2, "<StockItem>", GTK_STOCK_SAVE_AS},
  {N_("/File/Separator") , NULL, NULL, 0, "<Separator>"},
  {N_("/File/E_xit") , "<ctrl>Q", gtk_main_quit, 0, "<StockItem>", GTK_STOCK_QUIT},

  {N_("/Edit/_Paste") , "<ctrl><shift>v", (GtkItemFactoryCallback)gui_paste, 0, "<StockItem>", GTK_STOCK_PASTE},
  {N_("/Edit/_Copy") , "<ctrl><shift>v", (GtkItemFactoryCallback)gui_copy, 0, "<StockItem>", GTK_STOCK_COPY},
  {N_("/Edit/Copy _All") , NULL, (GtkItemFactoryCallback)gui_copy_all_clipboard, 0, "<StockItem>", GTK_STOCK_SELECT_ALL},

  {N_("/_Configuration"), NULL, NULL, 0, "<Branch>"},
  {N_("/Configuration/_Port"), "<ctrl>S", (GtkItemFactoryCallback)Config_Port_Fenetre, 0, "<StockItem>", GTK_STOCK_PREFERENCES},
  {N_("/Configuration/_Main window"), NULL, (GtkItemFactoryCallback)Config_Terminal, 0, "<StockItem>", GTK_STOCK_SELECT_FONT},
  {N_("/Configuration/Local _echo"), NULL, (GtkItemFactoryCallback)Toggle_Echo, 0, "<CheckItem>"},
  {N_("/Configuration/_Macros"), NULL, (GtkItemFactoryCallback)Config_macros, 0, "<Item>"},
  {N_("/Configuration/Separator") , NULL, NULL, 0, "<Separator>"},
  {N_("/Configuration/_Load configuration"), NULL, (GtkItemFactoryCallback)config_window, 0, "<StockItem>", GTK_STOCK_OPEN},
  {N_("/Configuration/_Save configuration"), NULL, (GtkItemFactoryCallback)config_window, 1, "<StockItem>", GTK_STOCK_SAVE_AS},
  {N_("/Configuration/_Delete configuration"), NULL, (GtkItemFactoryCallback)config_window, 2, "<StockItem>", GTK_STOCK_DELETE},
  {N_("/Control _signals"), NULL, NULL, 0, "<Branch>"},
  {N_("/Control signals/Send break"), "<ctrl>B", (GtkItemFactoryCallback)signaux, 2, "<Item>"},
  {N_("/Control signals/Toggle DTR"), "F7", (GtkItemFactoryCallback)signaux, 0, "<Item>"},
  {N_("/Control signals/Toggle RTS"), "F8", (GtkItemFactoryCallback)signaux, 1, "<Item>"},
  {N_("/_View"), NULL, NULL, 0, "<Branch>"},
  {N_("/View/_ASCII"), NULL, (GtkItemFactoryCallback)view, ASCII_VIEW, "<RadioItem>"},
  {N_("/View/_Hexadecimal"), NULL, (GtkItemFactoryCallback)view, HEXADECIMAL_VIEW, "<RadioItem>"},
  {N_("/View/Hexadecimal _chars"), NULL, NULL, 0, "<Branch>"},
  {N_("/View/Hexadecimal chars/_8"), NULL, (GtkItemFactoryCallback)hexadecimal_chars_to_display, 8, "<RadioItem>"},
  {N_("/View/Hexadecimal chars/1_0"), NULL, (GtkItemFactoryCallback)hexadecimal_chars_to_display, 10, "/View/Hexadecimal chars/8"},
  {N_("/View/Hexadecimal chars/_16"), NULL, (GtkItemFactoryCallback)hexadecimal_chars_to_display, 16, "/View/Hexadecimal chars/8"},
  {N_("/View/Hexadecimal chars/_24"), NULL, (GtkItemFactoryCallback)hexadecimal_chars_to_display, 24, "/View/Hexadecimal chars/8"},
  {N_("/View/Hexadecimal chars/_32"), NULL, (GtkItemFactoryCallback)hexadecimal_chars_to_display, 32, "/View/Hexadecimal chars/8"},
  {N_("/View/Show _index"), NULL, (GtkItemFactoryCallback)toggle_index, 0, "<CheckItem>"},
  {N_("/View/Separator") , NULL, NULL, 0, "<Separator>"},
  {N_("/View/_Send hexadecimal data") , NULL, (GtkItemFactoryCallback)show_hide_hex, 0, "<CheckItem>"},
  {N_("/_Help"), NULL, NULL, 0, "<LastBranch>"},
  {N_("/Help/_About..."), NULL, (GtkItemFactoryCallback)a_propos, 0, "<StockItem>", GTK_STOCK_DIALOG_INFO}
};

static gchar *translate_menu(const gchar *path, gpointer data)
{
  return _(path);
}

gint show_hide_hex(gpointer *pointer, guint param, GtkWidget *widget)
{
  if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
    gtk_widget_show(GTK_WIDGET(Hex_Box));
  else
    gtk_widget_hide(GTK_WIDGET(Hex_Box));

  return FALSE;
}

gint toggle_index(gpointer *pointer, guint param, GtkWidget *widget)
{
  show_index = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
  set_view(HEXADECIMAL_VIEW);
  return FALSE;
}

gint hexadecimal_chars_to_display(gpointer *pointer, guint param, GtkWidget *widget)
{
  bytes_per_line = param;
  set_view(HEXADECIMAL_VIEW);
  return FALSE;
}

void set_view(guint type)
{
  clear_display();
  set_clear_func(clear_display);
  switch(type)
    {
    case ASCII_VIEW:
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(hex_menu), FALSE);
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ascii_menu), TRUE);
      gtk_widget_set_sensitive(GTK_WIDGET(show_index_menu), FALSE);
      gtk_widget_set_sensitive(GTK_WIDGET(hex_chars_menu), FALSE);
      total_bytes = 0;
      set_display_func(put_text);
      break;
    case HEXADECIMAL_VIEW:
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(hex_menu), TRUE);
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ascii_menu), FALSE);
      gtk_widget_set_sensitive(GTK_WIDGET(show_index_menu), TRUE);
      gtk_widget_set_sensitive(GTK_WIDGET(hex_chars_menu), TRUE);
      total_bytes = 0;
      set_display_func(put_hexadecimal);
      break;
    default:
      set_display_func(NULL);
    }
  write_buffer();
}

gint view(gpointer *pointer, guint param, GtkWidget *widget)
{
  if(!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
    return FALSE;

  set_view(param);

  return FALSE;
}

void Set_local_echo(gboolean echo)
{
  echo_on = echo;
  if(echo_menu)
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(echo_menu), echo_on);
}

gint Toggle_Echo(gpointer *pointer, guint param, GtkWidget *widget)
{
  echo_on = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
  configure_echo(echo_on);
  return 0;
}


void create_main_window(void)
{
  GtkWidget *Menu, *Boite, *BoiteH, *Label;
  GtkWidget *Hex_Send_Entry;
  GtkItemFactory *item_factory;
  GtkAccelGroup *accel_group;
  GSList *group;

  Fenetre = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  shortcuts = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(Fenetre), GTK_ACCEL_GROUP(shortcuts));

  gtk_signal_connect(GTK_OBJECT(Fenetre), "destroy", (GtkSignalFunc)gtk_main_quit, NULL);
  gtk_signal_connect(GTK_OBJECT(Fenetre), "delete_event", (GtkSignalFunc)gtk_main_quit, NULL);
  gtk_window_set_title(GTK_WINDOW(Fenetre), "GtkTerm");

  Boite = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(Fenetre), Boite);

  accel_group = gtk_accel_group_new();
  item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", accel_group);
  gtk_item_factory_set_translate_func(item_factory, translate_menu, "<main>", NULL);
  gtk_window_add_accel_group(GTK_WINDOW(Fenetre), accel_group);
  gtk_item_factory_create_items(item_factory, NUMBER_OF_ITEMS, Tableau_Menu, NULL);
  Menu = gtk_item_factory_get_widget(item_factory, "<main>");
  echo_menu = gtk_item_factory_get_item(item_factory, "/Configuration/Local echo");
  ascii_menu = gtk_item_factory_get_item(item_factory, "/View/ASCII");
  hex_menu = gtk_item_factory_get_item(item_factory, "/View/Hexadecimal");
  hex_chars_menu = gtk_item_factory_get_item(item_factory, "/View/Hexadecimal chars");
  show_index_menu = gtk_item_factory_get_item(item_factory, "/View/Show index");
  group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(ascii_menu));
  gtk_radio_menu_item_set_group(GTK_RADIO_MENU_ITEM(hex_menu), group);

  hex_len_menu = gtk_item_factory_get_item(item_factory, "/View/Hexadecimal chars/16");
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(hex_len_menu), TRUE);

  gtk_box_pack_start(GTK_BOX(Boite), Menu, FALSE, TRUE, 0);

  BoiteH = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(Boite), BoiteH, TRUE, TRUE, 0);

  /* create vte window */
  display = vte_terminal_new();

  vte_terminal_set_backspace_binding(VTE_TERMINAL(display),
				     VTE_ERASE_ASCII_BACKSPACE);

  clear_display();

  /* make vte window scrollable - inspired by gnome-terminal package */
  scrolled_window = gtk_scrolled_window_new(NULL,
					    vte_terminal_get_adjustment(VTE_TERMINAL(display)));
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				 GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window),
				      GTK_SHADOW_NONE);
  gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(display));

  gtk_box_pack_start_defaults(GTK_BOX(BoiteH), scrolled_window);

  /* status bar */
  Hex_Box = gtk_hbox_new(TRUE, 0);
  Label = gtk_label_new(_("Hexadecimal data to send (separator : ';' or space) : "));
  gtk_box_pack_start_defaults(GTK_BOX(Hex_Box), Label);
  Hex_Send_Entry = gtk_entry_new();
  gtk_signal_connect(GTK_OBJECT(Hex_Send_Entry), "activate", (GtkSignalFunc)Send_Hexadecimal, NULL);
  gtk_box_pack_start(GTK_BOX(Hex_Box), Hex_Send_Entry, FALSE, TRUE, 5);
  gtk_box_pack_start(GTK_BOX(Boite), Hex_Box, FALSE, TRUE, 2);

  BoiteH = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(Boite), BoiteH, FALSE, FALSE, 0);

  StatusBar = gtk_statusbar_new();
  gtk_box_pack_start(GTK_BOX(BoiteH), StatusBar, TRUE, TRUE, 0);
  id = gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "Messages");

  Label = gtk_label_new("RI");
  gtk_box_pack_end(GTK_BOX(BoiteH), Label, FALSE, TRUE, 5);
  gtk_widget_set_sensitive(GTK_WIDGET(Label), FALSE);
  signals[0] = Label;

  Label = gtk_label_new("DSR");
  gtk_box_pack_end(GTK_BOX(BoiteH), Label, FALSE, TRUE, 5);
  signals[1] = Label;

  Label = gtk_label_new("CD");
  gtk_box_pack_end(GTK_BOX(BoiteH), Label, FALSE, TRUE, 5);
  signals[2] = Label;

  Label = gtk_label_new("CTS");
  gtk_box_pack_end(GTK_BOX(BoiteH), Label, FALSE, TRUE, 5);
  signals[3] = Label;

  Label = gtk_label_new("RTS");
  gtk_box_pack_end(GTK_BOX(BoiteH), Label, FALSE, TRUE, 5);
  signals[4] = Label;

  Label = gtk_label_new("DTR");
  gtk_box_pack_end(GTK_BOX(BoiteH), Label, FALSE, TRUE, 5);
  signals[5] = Label;

  g_signal_connect_after(GTK_OBJECT(display), "commit", G_CALLBACK(Got_Input), NULL);

  gtk_timeout_add(POLL_DELAY, (GtkFunction)control_signals_read, NULL);

  gtk_window_set_default_size(GTK_WINDOW(Fenetre), 750, 550);
  gtk_widget_show_all(Fenetre);
  gtk_widget_hide(GTK_WIDGET(Hex_Box));

}

void initialize_hexadecimal_display(void)
{
  total_bytes = 0;
  memset(blank_data, ' ', 128);
  blank_data[bytes_per_line * 3 + 5] = 0;
}

void put_hexadecimal(gchar *string, guint size)
{
  static gchar data[128];
  static gchar data_byte[6];
  static guint bytes;
  glong column, row;

  gint i = 0;

  if(size == 0)
    return;

  while(i < size)
    {
      while(gtk_events_pending()) gtk_main_iteration();
      vte_terminal_get_cursor_position(VTE_TERMINAL(display), &column, &row);

      if(show_index)
	{
	  if(column == 0)
	    /* First byte on line */
	    {
	      sprintf(data, "%6d: ", total_bytes);
	      vte_terminal_feed(VTE_TERMINAL(display), data, strlen(data));
	      bytes = 0;
	    }
	}
      else
	{
	  if(column == 0)
	    bytes = 0;
	}

      /* Print hexadecimal characters */
      data[0] = 0;

      while(bytes < bytes_per_line && i < size)
	{
	  gint avance=0;
	  gchar ascii[1];

	  sprintf(data_byte, "%02X ", (guchar)string[i]);
	  vte_terminal_feed(VTE_TERMINAL(display), data_byte, 3);

	  avance = (bytes_per_line - bytes) * 3 + bytes + 2;

	  /* Move forward */
	  sprintf(data_byte, "%c[%dC", 27, avance);
	  vte_terminal_feed(VTE_TERMINAL(display), data_byte, strlen(data_byte));

	  /* Print ascii characters */
	  ascii[0] = (string[i] > 0x1F) ? string[i] : '.';
	  vte_terminal_feed(VTE_TERMINAL(display), ascii, 1);

	  /* Move backward */
	  sprintf(data_byte, "%c[%dD", 27, avance + 1);
	  vte_terminal_feed(VTE_TERMINAL(display), data_byte, strlen(data_byte));

	  if(bytes == bytes_per_line / 2 - 1)
	    vte_terminal_feed(VTE_TERMINAL(display), "- ", strlen("- "));

	  bytes++;
	  i++;

	  /* End of line ? */
	  if(bytes == bytes_per_line)
	    {
	      vte_terminal_feed(VTE_TERMINAL(display), "\r\n", 2);
	      total_bytes += bytes;
	    }

	}

    }
}

void put_text(gchar *string, guint size)
{
  vte_terminal_feed(VTE_TERMINAL(display), string, size);
}

gint send_serial(gchar *string, gint len)
{
  gint bytes_written;

  bytes_written = Send_chars(string, len);
  if(bytes_written > 0)
    {
      if(echo_on)
	put_chars(string, bytes_written);
    }

  return bytes_written;
}


static void Got_Input(VteTerminal *widget, gchar *text, guint length, gpointer ptr)
{
  send_serial(text, length);
}

gboolean Envoie_car(GtkWidget *widget, GdkEventKey *event, gpointer pointer)
{
  if(g_utf8_validate(event->string, 1, NULL))
    send_serial(event->string, 1);

  return FALSE;
}


gint a_propos(GtkWidget *widget, guint param)
{
  GtkWidget *Dialogue, *Label, *Bouton;
  gchar *chaine;

  Dialogue = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(Dialogue), _("About..."));
  Bouton = gtk_button_new_from_stock (GTK_STOCK_OK);
  gtk_signal_connect_object(GTK_OBJECT(Bouton), "clicked", (GtkSignalFunc)gtk_widget_destroy, GTK_OBJECT(Dialogue));
  gtk_signal_connect(GTK_OBJECT(Dialogue), "destroy", (GtkSignalFunc)gtk_widget_destroy, NULL);
  gtk_signal_connect(GTK_OBJECT(Dialogue), "delete_event", (GtkSignalFunc)gtk_widget_destroy, NULL);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Dialogue)->action_area), Bouton, TRUE, TRUE, 0);

  Label = gtk_label_new("");
  chaine = g_strdup_printf(_("\n <big><i> GTKTerm V. %s </i></big> \n\n\t(c) Julien Schmitt\n\thttp://www.jls-info.com/julien/linux\n\n\tLatest Version Available on:\n\thttps://fedorahosted.org/gtkterm/"), VERSION);
  gtk_label_set_markup(GTK_LABEL(Label), chaine);
  g_free(chaine);
  gtk_label_set_selectable(GTK_LABEL(Label), TRUE);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Dialogue)->vbox), Label, TRUE, TRUE, 0);

  gtk_widget_show_all(Dialogue);

  return FALSE;
}

void show_control_signals(int stat)
{
  if(stat & TIOCM_RI)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[0]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[0]), FALSE);
  if(stat & TIOCM_DSR)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[1]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[1]), FALSE);
  if(stat & TIOCM_CD)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[2]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[2]), FALSE);
  if(stat & TIOCM_CTS)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[3]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[3]), FALSE);
  if(stat & TIOCM_RTS)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[4]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[4]), FALSE);
  if(stat & TIOCM_DTR)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[5]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[5]), FALSE);
}

gint signaux(GtkWidget *widget, guint param)
{
  if(param == 2)
    {
      sendbreak();
      Put_temp_message(_("Break signal sent !"), 800);
    }
  else
    Set_signals(param);
  return FALSE;
}

gboolean control_signals_read(void)
{
  int state;

  state = lis_sig();
  if(state >= 0)
    show_control_signals(state);

  return TRUE;
}

void Set_status_message(gchar *msg)
{
  gtk_statusbar_pop(GTK_STATUSBAR(StatusBar), id);
  gtk_statusbar_push(GTK_STATUSBAR(StatusBar), id, msg);
}

void show_message(gchar *message, gint type_msg)
{
 GtkWidget *Fenetre_msg;

 if(type_msg==MSG_ERR)
   {
     Fenetre_msg =
       gtk_message_dialog_new(GTK_WINDOW(Fenetre), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, message);
   }
 else if(type_msg==MSG_WRN)
   {
     Fenetre_msg =
       gtk_message_dialog_new(GTK_WINDOW(Fenetre), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, message);
   }
 else
   return;

 gtk_dialog_run(GTK_DIALOG(Fenetre_msg));
 gtk_widget_destroy(Fenetre_msg);
}

gboolean Send_Hexadecimal(GtkWidget *widget, GdkEventKey *event, gpointer pointer)
{
  gint i;
  gchar *text, *current;
  gchar *message;
  guchar val;
  guint val_read;
  guint sent = 0;
  gchar written[4];
  gchar *all_written;

  text = (gchar *)gtk_entry_get_text(GTK_ENTRY(widget));

  if(strlen(text) == 0){
      message = g_strdup_printf(_("0 byte(s) sent !"));
      Put_temp_message(message, 1500);
      gtk_entry_set_text(GTK_ENTRY(widget), "");
      g_free(message);
      return FALSE;
  }

  all_written = g_malloc(strlen(text) * 2 + 1);
  all_written[0] = 0;

  current = text;
  i = 0;
  while(i < strlen(text))
    {
      if(sscanf(current, "%02X", &val_read) == 1)
	{
	  val = (guchar)val_read;
	  send_serial((gchar*)&val, 1);
	  sprintf(written, "%02X ", val);
	  strcat(all_written, written);
	  sent++;
	}
      while(i < strlen(text) && text[i] != ';' && text[i] != ' ')
	i++;
      if(text[i] == ';' || text[i] == ' ')
	{
	  i++;
	  current = &text[i];
	}
    }
  all_written[strlen(all_written) - 1] = 0;
  message = g_strdup_printf(_("\"%s\" : %d byte(s) sent !"), all_written, sent);
  Put_temp_message(message, 1500);
  gtk_entry_set_text(GTK_ENTRY(widget), "");
  g_free(message);
  g_free(all_written);

  return FALSE;
}

void Put_temp_message(const gchar *text, gint time)
{
  /* time in ms */
  gtk_statusbar_push(GTK_STATUSBAR(StatusBar), id, text);
  gtk_timeout_add(time, (GtkFunction)pop_message, NULL);
}

gboolean pop_message(void)
{
  gtk_statusbar_pop(GTK_STATUSBAR(StatusBar), id);

  return FALSE;
}

void clear_display(void)
{
  initialize_hexadecimal_display();
  if(display)
    vte_terminal_reset(VTE_TERMINAL(display), TRUE, TRUE);
}

gint gui_paste(void)
{
    vte_terminal_paste_clipboard(VTE_TERMINAL(display));
    return 0;
}

gint gui_copy(void)
{
    vte_terminal_copy_clipboard(VTE_TERMINAL(display));
    return 0;
}

gint gui_copy_all_clipboard(void)
{
    vte_terminal_select_all(VTE_TERMINAL(display));
    gui_copy();
    vte_terminal_select_none(VTE_TERMINAL(display));

    return 0;
}
