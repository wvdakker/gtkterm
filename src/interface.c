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
/*      - 0.99.7 : Changed keyboard shortcuts to <ctrl><shift>         */
/*	            (Ken Peek)                                         */
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "term_config.h"
#include "files.h"
#include "serial.h"
#include "interface.h"
#include "buffer.h"
#include "macros.h"
#include "auto_config.h"
#include "logging.h"

#include <config.h>
#include <glib/gi18n.h>

guint id;
gboolean echo_on;
gboolean crlfauto_on;
GtkWidget *StatusBar;
GtkWidget *signals[6];
static GtkWidget *Hex_Box;
static GtkWidget *log_pause_resume_menu = NULL;
static GtkWidget *log_start_menu = NULL;
static GtkWidget *log_stop_menu = NULL;
static GtkWidget *log_clear_menu = NULL;
GtkWidget *scrolled_window;
GtkWidget *Fenetre;
GtkWidget *popup_menu;
GtkUIManager *ui_manager;
GtkAccelGroup *shortcuts;
GtkActionGroup *action_group;
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
void signals_send_break_callback(GtkAction *action, gpointer data);
void signals_toggle_DTR_callback(GtkAction *action, gpointer data);
void signals_toggle_RTS_callback(GtkAction *action, gpointer data);
void signals_close_port(GtkAction *action, gpointer data);
void signals_open_port(GtkAction *action, gpointer data);
void help_about_callback(GtkAction *action, gpointer data);
gboolean Envoie_car(GtkWidget *, GdkEventKey *, gpointer);
gboolean control_signals_read(void);
void echo_toggled_callback(GtkAction *action, gpointer data);
void CR_LF_auto_toggled_callback(GtkAction *action, gpointer data);
void view_radio_callback(GtkAction *action, gpointer data);
void view_hexadecimal_chars_radio_callback(GtkAction* action, gpointer data);
void view_index_toggled_callback(GtkAction *action, gpointer data);
void view_send_hex_toggled_callback(GtkAction *action, gpointer data);
void initialize_hexadecimal_display(void);
gboolean Send_Hexadecimal(GtkWidget *, GdkEventKey *, gpointer);
gboolean pop_message(void);
static gchar *translate_menu(const gchar *, gpointer);
static void Got_Input(VteTerminal *, gchar *, guint, gpointer);
void edit_copy_callback(GtkAction *action, gpointer data);
void update_copy_sensivity(VteTerminal *terminal, gpointer data);
void edit_paste_callback(GtkAction *action, gpointer data);
void edit_select_all_callback(GtkAction *action, gpointer data);


/* Menu */
const GtkActionEntry menu_entries[] =
{
	/* Toplevel */
	{"File", NULL, N_("_File")},
	{"Edit", NULL, N_("_Edit")},
	{"Log", NULL, N_("_Log")},
	{"Configuration", NULL, N_("_Configuration")},
	{"Signals", NULL, N_("Control _signals")},
	{"View", NULL, N_("_View")},
	{"ViewHexadecimalChars", NULL, N_("Hexadecimal _chars")},
	{"Help", NULL, N_("_Help")},

	/* File menu */
	{"FileExit", GTK_STOCK_QUIT, NULL, "<shift><control>Q", NULL, gtk_main_quit},
	{"ClearScreen", GTK_STOCK_CLEAR, N_("_Clear screen"), "<shift><control>L", NULL, G_CALLBACK(clear_buffer)},
	{"SendFile", GTK_STOCK_JUMP_TO, N_("Send _RAW file"), "<shift><control>R", NULL, G_CALLBACK(send_raw_file)},
	{"SaveFile", GTK_STOCK_SAVE_AS, N_("_Save RAW file"), "", NULL, G_CALLBACK(save_raw_file)},

	/* Edit menu */
	{"EditCopy", GTK_STOCK_COPY, NULL, "<shift><control>C", NULL, G_CALLBACK(edit_copy_callback)},
	{"EditPaste", GTK_STOCK_PASTE, NULL, "<shift><control>V", NULL, G_CALLBACK(edit_paste_callback)},
	{"EditSelectAll", GTK_STOCK_SELECT_ALL, NULL, "<shift><control>A", NULL, G_CALLBACK(edit_select_all_callback)},

	/* Log Menu */
	{"LogToFile", GTK_STOCK_MEDIA_RECORD, N_("To file..."), "", NULL, G_CALLBACK(logging_start)},
	{"LogPauseResume", GTK_STOCK_MEDIA_PAUSE, NULL, "", NULL, G_CALLBACK(logging_pause_resume)},
	{"LogStop", GTK_STOCK_MEDIA_STOP, NULL, "", NULL, G_CALLBACK(logging_stop)},
	{"LogClear", GTK_STOCK_CLEAR, NULL, "", NULL, G_CALLBACK(logging_clear)},

	/* Confuguration Menu */
	{"ConfigPort", GTK_STOCK_PROPERTIES, N_("_Port"), "<shift><control>S", NULL, G_CALLBACK(Config_Port_Fenetre)},
	{"ConfigTerminal", GTK_STOCK_PREFERENCES, N_("_Main window"), "", NULL, G_CALLBACK(Config_Terminal)},
	{"Macros", NULL, N_("_Macros"), NULL, NULL, G_CALLBACK(Config_macros)},
	{"SelectConfig", GTK_STOCK_OPEN, N_("_Load configuration"), "", NULL, G_CALLBACK(select_config_callback)},
	{"SaveConfig", GTK_STOCK_SAVE_AS, N_("_Save configuration"), "", NULL, G_CALLBACK(save_config_callback)},
	{"DeleteConfig", GTK_STOCK_DELETE, N_("_Delete configuration"), "", NULL, G_CALLBACK(delete_config_callback)},

	/* Signals Menu */
	{"SignalsSendBreak", NULL, N_("Send break"), "<shift><control>B", NULL, G_CALLBACK(signals_send_break_callback)},
	{"SignalsOpenPort", GTK_STOCK_OPEN, N_("_Open Port"), "F5", NULL, G_CALLBACK(signals_open_port)},
	{"SignalsClosePort", GTK_STOCK_CLOSE, N_("_Close Port"), "F6", NULL, G_CALLBACK(signals_close_port)},
	{"SignalsDTR", NULL, N_("Toggle DTR"), "F7", NULL, G_CALLBACK(signals_toggle_DTR_callback)},
	{"SignalsRTS", NULL, N_("Toggle RTS"), "F8", NULL, G_CALLBACK(signals_toggle_RTS_callback)},

	/* About menu */
	{"HelpAbout", GTK_STOCK_ABOUT, NULL, NULL, NULL, G_CALLBACK(help_about_callback)}
};

const GtkToggleActionEntry menu_toggle_entries[] =
{
	/* Configuration Menu */
	{"LocalEcho", NULL, N_("Local _echo"), NULL, NULL, G_CALLBACK(echo_toggled_callback), FALSE},
	{"CRLFauto", NULL, N_("_CR LF auto"), NULL, NULL, G_CALLBACK(CR_LF_auto_toggled_callback), FALSE},

	/* View Menu */
	{"ViewIndex", NULL, N_("Show _index"), NULL, NULL, G_CALLBACK(view_index_toggled_callback), FALSE},
	{"ViewSendHexData", NULL, N_("_Send hexadecimal data"), NULL, NULL, G_CALLBACK(view_send_hex_toggled_callback), FALSE}
};

const GtkRadioActionEntry menu_view_radio_entries[] =
{
	{"ViewASCII", NULL, N_("_ASCII"), NULL, NULL, ASCII_VIEW},
	{"ViewHexadecimal", NULL, N_("_Hexadecimal"), NULL, NULL, HEXADECIMAL_VIEW}
};

const GtkRadioActionEntry menu_hex_chars_length_radio_entries[] =
{
	{"ViewHex8", NULL, "_8", NULL, NULL, 8},
	{"ViewHex10", NULL, "1_0", NULL, NULL, 10},
	{"ViewHex16", NULL, "_16", NULL, NULL, 16},
	{"ViewHex24", NULL, "_24", NULL, NULL, 24},
	{"ViewHex32", NULL, "_32", NULL, NULL, 32}
};

static const char *ui_description =
    "<ui>"
    "  <menubar name='MenuBar'>"
    "    <menu action='File'>"
    "      <menuitem action='ClearScreen'/>"
    "      <menuitem action='SendFile'/>"
    "      <menuitem action='SaveFile'/>"
    "      <separator/>"
    "      <menuitem action='FileExit'/>"
    "    </menu>"
    "    <menu action='Edit'>"
    "      <menuitem action='EditCopy'/>"
    "      <menuitem action='EditPaste'/>"
    "      <separator/>"
    "      <menuitem action='EditSelectAll'/>"
    "    </menu>"
    "    <menu action='Log'>"
    "      <menuitem action='LogToFile'/>"
    "      <menuitem action='LogPauseResume'/>"
    "      <menuitem action='LogStop'/>"
    "      <menuitem action='LogClear'/>"
    "    </menu>"
    "    <menu action='Configuration'>"
    "      <menuitem action='ConfigPort'/>"
    "      <menuitem action='ConfigTerminal'/>"
    "      <menuitem action='LocalEcho'/>"
    "      <menuitem action='CRLFauto'/>"
    "      <menuitem action='Macros'/>"
    "      <separator/>"
    "      <menuitem action='SelectConfig'/>"
    "      <menuitem action='SaveConfig'/>"
    "      <menuitem action='DeleteConfig'/>"
    "    </menu>"
    "    <menu action='Signals'>"
    "      <menuitem action='SignalsSendBreak'/>"
    "      <menuitem action='SignalsOpenPort'/>"
    "      <menuitem action='SignalsClosePort'/>"
    "      <menuitem action='SignalsDTR'/>"
    "      <menuitem action='SignalsRTS'/>"
    "    </menu>"
    "    <menu action='View'>"
    "      <menuitem action='ViewASCII'/>"
    "      <menuitem action='ViewHexadecimal'/>"
    "      <menu action='ViewHexadecimalChars'>"
    "        <menuitem action='ViewHex8'/>"
    "        <menuitem action='ViewHex10'/>"
    "        <menuitem action='ViewHex16'/>"
    "        <menuitem action='ViewHex24'/>"
    "        <menuitem action='ViewHex32'/>"
    "      </menu>"
    "      <menuitem action='ViewIndex'/>"
    "      <separator/>"
    "      <menuitem action='ViewSendHexData'/>"
    "    </menu>"
    "    <menu action='Help'>"
    "      <menuitem action='HelpAbout'/>"
    "    </menu>"
    "  </menubar>"
    "  <popup name='PopupMenu'>"
    "    <menuitem action='EditCopy'/>"
    "    <menuitem action='EditPaste'/>"
    "    <separator/>"
    "    <menuitem action='EditSelectAll'/>"
    "  </popup>"
    "</ui>";

static gchar *translate_menu(const gchar *path, gpointer data)
{
	return _(path);
}

void view_send_hex_toggled_callback(GtkAction *action, gpointer data)
{
	if(gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action)))
		gtk_widget_show(GTK_WIDGET(Hex_Box));
	else
		gtk_widget_hide(GTK_WIDGET(Hex_Box));
}

void view_index_toggled_callback(GtkAction *action, gpointer data)
{
	show_index = gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action));
	set_view(HEXADECIMAL_VIEW);
}

void view_hexadecimal_chars_radio_callback(GtkAction* action, gpointer data)
{
	gint current_value;
	current_value = gtk_radio_action_get_current_value(GTK_RADIO_ACTION(action));

	bytes_per_line = current_value;
	set_view(HEXADECIMAL_VIEW);
}

void set_view(guint type)
{
	GtkAction *action;
	GtkAction *show_index_action;
	GtkAction *hex_chars_action;

	show_index_action = gtk_action_group_get_action(action_group, "ViewIndex");
	hex_chars_action = gtk_action_group_get_action(action_group, "ViewHexadecimalChars");

	clear_display();
	set_clear_func(clear_display);
	switch(type)
	{
	case ASCII_VIEW:
		action = gtk_action_group_get_action(action_group, "ViewASCII");
		gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), TRUE);
		gtk_action_set_sensitive(show_index_action, FALSE);
		gtk_action_set_sensitive(hex_chars_action, FALSE);
		total_bytes = 0;
		set_display_func(put_text);
		break;
	case HEXADECIMAL_VIEW:
		action = gtk_action_group_get_action(action_group, "ViewHexadecimal");
		gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), TRUE);
		gtk_action_set_sensitive(show_index_action, TRUE);
		gtk_action_set_sensitive(hex_chars_action, TRUE);
		total_bytes = 0;
		set_display_func(put_hexadecimal);
		break;
	default:
		set_display_func(NULL);
	}
	write_buffer();
}

void view_radio_callback(GtkAction *action, gpointer data)
{
	gint current_value;
	current_value = gtk_radio_action_get_current_value(GTK_RADIO_ACTION(action));

	set_view(current_value);
}

void Set_local_echo(gboolean echo)
{
	GtkAction *action;

	echo_on = echo;

	action = gtk_action_group_get_action(action_group, "LocalEcho");
	if(action)
		gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), echo_on);
}

void echo_toggled_callback(GtkAction *action, gpointer data)
{
	echo_on = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION(action));
	configure_echo(echo_on);
}

void Set_crlfauto(gboolean crlfauto)
{
	GtkAction *action;

	crlfauto_on = crlfauto;

	action = gtk_action_group_get_action(action_group, "CRLFauto");
	if(action)
		gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), crlfauto_on);
}

void CR_LF_auto_toggled_callback(GtkAction *action, gpointer data)
{
	crlfauto_on = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION(action));
	configure_crlfauto(crlfauto_on);
}

void toggle_logging_pause_resume(gboolean currentlyLogging)
{
	GtkAction *action;

	action = gtk_action_group_get_action(action_group, "LogPauseResume");

	if (currentlyLogging)
	{
		gtk_action_set_label(action, NULL);
		gtk_action_set_stock_id(action, GTK_STOCK_MEDIA_PAUSE);
	}
	else
	{
		gtk_action_set_label(action, _("Resume"));
		gtk_action_set_stock_id(action, GTK_STOCK_MEDIA_PLAY);
	}
}

void toggle_logging_sensitivity(gboolean currentlyLogging)
{
	GtkAction *action;

	action = gtk_action_group_get_action(action_group, "LogToFile");
	gtk_action_set_sensitive(action, !currentlyLogging);
	action = gtk_action_group_get_action(action_group, "LogPauseResume");
	gtk_action_set_sensitive(action, currentlyLogging);
	action = gtk_action_group_get_action(action_group, "LogStop");
	gtk_action_set_sensitive(action, currentlyLogging);
	action = gtk_action_group_get_action(action_group, "LogClear");
	gtk_action_set_sensitive(action, currentlyLogging);
}

gboolean terminal_button_press_callback(GtkWidget *widget,
                                        GdkEventButton *event,
                                        gpointer *data)
{

	if(event->type == GDK_BUTTON_PRESS &&
	        event->button == 3 &&
	        (event->state & gtk_accelerator_get_default_mod_mask()) == 0)
	{
		gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, NULL, NULL,
		               event->button, event->time);
		return TRUE;
	}

	return FALSE;
}

void terminal_popup_menu_callback(GtkWidget *widget, gpointer data)
{
	gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, NULL, NULL,
	               0, gtk_get_current_event_time());
}

void create_main_window(void)
{
	GtkWidget *menu, *main_vbox, *label;
	GtkWidget *hex_send_entry;
	GtkAccelGroup *accel_group;
	GError *error;

	Fenetre = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	shortcuts = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(Fenetre), GTK_ACCEL_GROUP(shortcuts));

	g_signal_connect(GTK_WIDGET(Fenetre), "destroy", (GCallback)gtk_main_quit, NULL);
	g_signal_connect(GTK_WIDGET(Fenetre), "delete_event", (GCallback)gtk_main_quit, NULL);

	Set_window_title("GtkTerm");

	main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(Fenetre), main_vbox);

	/* Create the UIManager */
	ui_manager = gtk_ui_manager_new();

	accel_group = gtk_ui_manager_get_accel_group (ui_manager);
	gtk_window_add_accel_group (GTK_WINDOW (Fenetre), accel_group);

	/* Create the actions */
	action_group = gtk_action_group_new("MenuActions");
	gtk_action_group_set_translate_func(action_group, translate_menu, NULL, NULL);

	gtk_action_group_add_actions(action_group, menu_entries,
	                             G_N_ELEMENTS (menu_entries),
	                             Fenetre);
	gtk_action_group_add_toggle_actions(action_group, menu_toggle_entries,
	                                    G_N_ELEMENTS (menu_toggle_entries),
	                                    Fenetre);
	gtk_action_group_add_radio_actions(action_group, menu_view_radio_entries,
	                                   G_N_ELEMENTS (menu_view_radio_entries),
	                                   -1, G_CALLBACK(view_radio_callback),
	                                   Fenetre);
	gtk_action_group_add_radio_actions(action_group, menu_hex_chars_length_radio_entries,
	                                   G_N_ELEMENTS (menu_hex_chars_length_radio_entries),
	                                   16, G_CALLBACK(view_hexadecimal_chars_radio_callback),
	                                   Fenetre);

	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

	/* Load the UI */
	error = NULL;
	if(!gtk_ui_manager_add_ui_from_string(ui_manager, ui_description, -1, &error))
	{
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
		exit (EXIT_FAILURE);
	}

	menu = gtk_ui_manager_get_widget (ui_manager, "/MenuBar");
	gtk_box_pack_start(GTK_BOX(main_vbox), menu, FALSE, TRUE, 0);

	/* create vte window */
	display = vte_terminal_new();

	/* set terminal properties, these could probably be made user configurable */
	vte_terminal_set_scroll_on_output(VTE_TERMINAL(display), FALSE);
	vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(display), TRUE);
	vte_terminal_set_mouse_autohide(VTE_TERMINAL(display), TRUE);
	vte_terminal_set_backspace_binding(VTE_TERMINAL(display),
	                                   VTE_ERASE_ASCII_BACKSPACE);

	clear_display();

	/* make vte window scrollable - inspired by gnome-terminal package */
	scrolled_window = gtk_scrolled_window_new(NULL, gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (display)));

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
	                               GTK_POLICY_AUTOMATIC,
	                               GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window),
	                                    GTK_SHADOW_NONE);

	gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(display));

	gtk_box_pack_start(GTK_BOX(main_vbox), scrolled_window, TRUE, TRUE, 0);

	g_signal_connect(G_OBJECT(display), "button-press-event",
	                 G_CALLBACK(terminal_button_press_callback), NULL);

	g_signal_connect(G_OBJECT(display), "popup-menu",
	                 G_CALLBACK(terminal_popup_menu_callback), NULL);

	g_signal_connect(G_OBJECT(display), "selection-changed",
	                 G_CALLBACK(update_copy_sensivity), NULL);
	update_copy_sensivity(VTE_TERMINAL(display), NULL);

	popup_menu = gtk_ui_manager_get_widget(ui_manager, "/PopupMenu");

	/* set up logging buttons availability */
	toggle_logging_pause_resume(FALSE);
	toggle_logging_sensitivity(FALSE);

	/* send hex char box (hidden when not in use) */
	Hex_Box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	label = gtk_label_new(_("Hexadecimal data to send (separator : ';' or space) : "));
	gtk_box_pack_start(GTK_BOX(Hex_Box), label, FALSE, FALSE, 5);
	hex_send_entry = gtk_entry_new();
	g_signal_connect(GTK_WIDGET(hex_send_entry), "activate", (GCallback)Send_Hexadecimal, NULL);
	gtk_box_pack_start(GTK_BOX(Hex_Box), hex_send_entry, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(main_vbox), Hex_Box, FALSE, TRUE, 2);

	/* status bar */
	StatusBar = gtk_statusbar_new();
	gtk_box_pack_start(GTK_BOX(main_vbox), StatusBar, FALSE, FALSE, 0);
	id = gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "Messages");

	label = gtk_label_new("RI");
	gtk_box_pack_end(GTK_BOX(StatusBar), label, FALSE, TRUE, 5);
	gtk_widget_set_sensitive(GTK_WIDGET(label), FALSE);
	signals[0] = label;

	label = gtk_label_new("DSR");
	gtk_box_pack_end(GTK_BOX(StatusBar), label, FALSE, TRUE, 5);
	signals[1] = label;

	label = gtk_label_new("CD");
	gtk_box_pack_end(GTK_BOX(StatusBar), label, FALSE, TRUE, 5);
	signals[2] = label;

	label = gtk_label_new("CTS");
	gtk_box_pack_end(GTK_BOX(StatusBar), label, FALSE, TRUE, 5);
	signals[3] = label;

	label = gtk_label_new("RTS");
	gtk_box_pack_end(GTK_BOX(StatusBar), label, FALSE, TRUE, 5);
	signals[4] = label;

	label = gtk_label_new("DTR");
	gtk_box_pack_end(GTK_BOX(StatusBar), label, FALSE, TRUE, 5);
	signals[5] = label;

	g_signal_connect_after(GTK_WIDGET(display), "commit", G_CALLBACK(Got_Input), NULL);

	g_timeout_add(POLL_DELAY, (GSourceFunc)control_signals_read, NULL);

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
			log_chars(data_byte, 3);
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
	log_chars(string, size);
	vte_terminal_feed(VTE_TERMINAL(display), string, size);
}

gint send_serial(gchar *string, gint len)
{
	gint bytes_written;

	bytes_written = Send_chars(string, len);
	if(bytes_written > 0)
	{
		if(echo_on)
			put_chars(string, bytes_written, crlfauto_on);
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


void help_about_callback(GtkAction *action, gpointer data)
{
	gchar *authors[] = {"Julien Schimtt", "Zach Davis", NULL};

	gtk_show_about_dialog(GTK_WINDOW(Fenetre),
	                      "program-name", "GTKTerm",
	                      "version", VERSION,
	                      "comments", _("GTKTerm is a simple GTK+ terminal used to communicate with the serial port."),
	                      "copyright", "Copyright © Julien Schimtt",
	                      "authors", authors,
	                      "website", "https://fedorahosted.org/gtkterm/",
	                      "website-label", "https://fedorahosted.org/gtkterm/",
	                      "license-type", GTK_LICENSE_LGPL_3_0,
	                      NULL);
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

void signals_send_break_callback(GtkAction *action, gpointer data)
{
	sendbreak();
	Put_temp_message(_("Break signal sent!"), 800);
}

void signals_toggle_DTR_callback(GtkAction *action, gpointer data)
{
	Set_signals(0);
}

void signals_toggle_RTS_callback(GtkAction *action, gpointer data)
{
	Set_signals(1);
}

void signals_close_port(GtkAction *action, gpointer data)
{
	Close_port();

	gchar *message;
	message = get_port_string();
	Set_status_message(message);
	Set_window_title(message);
	g_free(message);
}

void signals_open_port(GtkAction *action, gpointer data)
{
	Config_port();

	gchar *message;
	message = get_port_string();
	Set_status_message(message);
	Set_window_title(message);
	g_free(message);
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

void Set_window_title(gchar *msg)
{
	gchar* header = g_strdup_printf("GtkTerm - %s", msg);
	gtk_window_set_title(GTK_WINDOW(Fenetre), header);
	g_free(header);
}

void show_message(gchar *message, gint type_msg)
{
	GtkWidget *Fenetre_msg;

	if(type_msg==MSG_ERR)
	{
		Fenetre_msg = gtk_message_dialog_new(GTK_WINDOW(Fenetre),
		                                     GTK_DIALOG_DESTROY_WITH_PARENT,
		                                     GTK_MESSAGE_ERROR,
		                                     GTK_BUTTONS_OK,
		                                     message, NULL);
	}
	else if(type_msg==MSG_WRN)
	{
		Fenetre_msg = gtk_message_dialog_new(GTK_WINDOW(Fenetre),
		                                     GTK_DIALOG_DESTROY_WITH_PARENT,
		                                     GTK_MESSAGE_WARNING,
		                                     GTK_BUTTONS_OK,
		                                     message, NULL);
	}
	else
		return;

	gtk_dialog_run(GTK_DIALOG(Fenetre_msg));
	gtk_widget_destroy(Fenetre_msg);
}

gboolean Send_Hexadecimal(GtkWidget *widget, GdkEventKey *event, gpointer pointer)
{
	guint i;
	gchar *text, *message, **tokens, *buff;
	guint scan_val;

	text = (gchar *)gtk_entry_get_text(GTK_ENTRY(widget));

	if(strlen(text) == 0)
	{
		message = g_strdup_printf(_("0 byte(s) sent!"));
		Put_temp_message(message, 1500);
		gtk_entry_set_text(GTK_ENTRY(widget), "");
		g_free(message);
		return FALSE;
	}

	tokens = g_strsplit_set(text, " ;", -1);
	buff = g_malloc(g_strv_length(tokens));

	for(i = 0; tokens[i] != NULL; i++)
	{
		if(sscanf(tokens[i], "%02X", &scan_val) != 1)
		{
			Put_temp_message(_("Improper formatted hex input, 0 bytes sent!"),
			                 1500);
			g_free(buff);
			return FALSE;
		}
		buff[i] = scan_val;
	}

	send_serial(buff, i);
	g_free(buff);

	message = g_strdup_printf(_("%d byte(s) sent!"), i);
	Put_temp_message(message, 2000);
	gtk_entry_set_text(GTK_ENTRY(widget), "");
	g_strfreev(tokens);

	return FALSE;
}

void Put_temp_message(const gchar *text, gint time)
{
	/* time in ms */
	gtk_statusbar_push(GTK_STATUSBAR(StatusBar), id, text);
	g_timeout_add(time, (GSourceFunc)pop_message, NULL);
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

void edit_copy_callback(GtkAction *action, gpointer data)
{
	vte_terminal_copy_clipboard(VTE_TERMINAL(display));
}

void update_copy_sensivity(VteTerminal *terminal, gpointer data)
{
	GtkAction *action;
	gboolean can_copy;

	can_copy = vte_terminal_get_has_selection(VTE_TERMINAL(terminal));

	action = gtk_action_group_get_action(action_group, "EditCopy");
	gtk_action_set_sensitive(action, can_copy);
}

void edit_paste_callback(GtkAction *action, gpointer data)
{
	vte_terminal_paste_clipboard(VTE_TERMINAL(display));
}

void edit_select_all_callback(GtkAction *action, gpointer data)
{
	vte_terminal_select_all(VTE_TERMINAL(display));
}
