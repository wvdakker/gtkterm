/***********************************************************************/
/* config.c                                                            */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Configuration of the serial port                               */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.7 : Refactor to use newer gtk widgets                   */
/*                 Add ability to use arbitrary baud                   */
/*                 Add rs458 capability - Marc Le Douarain             */
/*                 Remove auto cr/lf stuff - (use macros instead)      */
/*      - 0.99.5 : Make the combo list for the device editable         */
/*      - 0.99.3 : Configuration for VTE terminal                      */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.99.1 : fixed memory management bug                         */
/*                 test if there are devices found                     */
/*      - 0.99.0 : fixed enormous memory management bug ;-)            */
/*                 save / read macros                                  */
/*      - 0.98.5 : font saved in configuration                         */
/*                 bug fixed in memory management                      */
/*                 combos set to non editable                          */
/*      - 0.98.3 : configuration file                                  */
/*      - 0.98.2 : autodetect existing devices                         */
/*      - 0.98 : added devfs devices                                   */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vte/vte.h>
#include <glib/gi18n.h>

#include "serie.h"
#include "term_config.h"
#include "widgets.h"
#include "parsecfg.h"
#include "macros.h"
#include "i18n.h"
#include "config.h"


#define DEVICE_NUMBERS_TO_CHECK 12

gchar *devices_to_check[] =
{
	"/dev/ttyS%d",
	"/dev/tts/%d",
	"/dev/ttyUSB%d",
	"/dev/ttyACM%d",
	"/dev/usb/tts/%d",
	NULL
};

/* Configuration file variables */
gchar **port;
gint *speed;
gint *bits;
gint *stopbits;
gchar **parity;
gchar **flow;
gint *wait_delay;
gint *wait_char;
gint *rts_time_before_tx;
gint *rts_time_after_tx;
gint *echo;
gint *crlfauto;
cfgList **macro_list = NULL;
gchar **font;

gint *show_cursor;
gint *rows;
gint *columns;
gint *scrollback;
gint *visual_bell;
gfloat *foreground_red;
gfloat *foreground_blue;
gfloat *foreground_green;
gfloat *foreground_alpha;
gfloat *background_red;
gfloat *background_blue;
gfloat *background_green;
gfloat *background_alpha;


cfgStruct cfg[] =
{
	{"port", CFG_STRING, &port},
	{"speed", CFG_INT, &speed},
	{"bits", CFG_INT, &bits},
	{"stopbits", CFG_INT, &stopbits},
	{"parity", CFG_STRING, &parity},
	{"flow", CFG_STRING, &flow},
	{"wait_delay", CFG_INT, &wait_delay},
	{"wait_char", CFG_INT, &wait_char},
	{"rs485_rts_time_before_tx", CFG_INT, &rts_time_before_tx},
	{"rs485_rts_time_after_tx", CFG_INT, &rts_time_after_tx},
	{"echo", CFG_BOOL, &echo},
	{"crlfauto", CFG_BOOL, &crlfauto},
	{"font", CFG_STRING, &font},
	{"macros", CFG_STRING_LIST, &macro_list},
	{"term_show_cursor", CFG_BOOL, &show_cursor},
	{"term_rows", CFG_INT, &rows},
	{"term_columns", CFG_INT, &columns},
	{"term_scrollback", CFG_INT, &scrollback},
	{"term_visual_bell", CFG_BOOL, &visual_bell},
	{"term_foreground_red", CFG_FLOAT, &foreground_red},
	{"term_foreground_blue", CFG_FLOAT, &foreground_blue},
	{"term_foreground_green", CFG_FLOAT, &foreground_green},
	{"term_foreground_alpha", CFG_FLOAT, &foreground_alpha},
	{"term_background_red", CFG_FLOAT, &background_red},
	{"term_background_blue", CFG_FLOAT, &background_blue},
	{"term_background_green", CFG_FLOAT, &background_green},
	{"term_background_alpha", CFG_FLOAT, &background_alpha},
	{NULL, CFG_END, NULL}
};

gchar *config_file;

struct configuration_port config;
display_config_t term_conf;

GtkWidget *Entry;

gint Grise_Degrise(GtkWidget *bouton, gpointer pointeur);
void read_font_button(GtkFontButton *fontButton);
void Hard_default_configuration(void);
void Copy_configuration(int);

static void Select_config(gchar *, void *);
static void Save_config_file(void);
static void load_config(GtkDialog *, gint, GtkTreeSelection *);
static void delete_config(GtkDialog *, gint, GtkTreeSelection *);
static void save_config(GtkDialog *, gint, GtkWidget *);
static void really_save_config(GtkDialog *, gint, gpointer);
static gint remove_section(gchar *, gchar *);
static void Curseur_OnOff(GtkWidget *, gpointer);
static void Selec_couleur(GdkRGBA *, gfloat, gfloat, gfloat, gfloat);
void config_fg_color(GtkWidget *button, gpointer data);
void config_bg_color(GtkWidget *button, gpointer data);
static gint scrollback_set(GtkWidget *, GdkEventFocus *, gpointer);

extern GtkWidget *display;

void Config_Port_Fenetre(GtkAction *action, gpointer data)
{
	GtkWidget *Table, *Label, *Bouton_OK, *Bouton_annule,
	          *Combo, *Dialogue, *Frame, *CheckBouton,
	          *Spin, *Expander, *ExpanderVbox,
	          *content_area, *action_area;

	static GtkWidget *Combos[10];
	GList *liste = NULL;
	gchar *chaine = NULL;
	gchar **dev = NULL;
	GtkAdjustment *adj;
	struct stat my_stat;
	gchar *string;
	int i;

	for(dev = devices_to_check; *dev != NULL; dev++)
	{
		for(i = 0; i < DEVICE_NUMBERS_TO_CHECK; i++)
		{
			chaine = g_strdup_printf(*dev, i);
			if(stat(chaine, &my_stat) == 0)
				liste = g_list_append(liste, chaine);
		}
	}

	if(liste == NULL)
	{
		show_message(_("No serial devices found!\n\n"
		               "Searched the following paths:\n"
		               "\t/dev/ttyS*\n\t/dev/tts/*\n\t/dev/ttyUSB*\n\t/dev/usb/tts/*\n\n"
		               "Enter a different device path in the 'Port' box.\n"), MSG_WRN);
	}

	Dialogue = gtk_dialog_new();
	content_area = gtk_dialog_get_content_area(GTK_DIALOG(Dialogue));
	action_area = gtk_dialog_get_action_area(GTK_DIALOG(Dialogue));
	gtk_window_set_title(GTK_WINDOW(Dialogue), _("Configuration"));
	gtk_window_set_resizable(GTK_WINDOW(Dialogue), FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(content_area), 5);

	Frame = gtk_frame_new(_("Serial port"));
	gtk_box_pack_start(GTK_BOX(content_area), Frame, FALSE, TRUE, 5);

	Table = gtk_table_new(4, 3, FALSE);
	gtk_container_add(GTK_CONTAINER(Frame), Table);

	Label = gtk_label_new(_("Port:"));
	gtk_table_attach(GTK_TABLE(Table), Label, 0, 1, 0, 1, 0, 0, 10, 5);
	Label = gtk_label_new(_("Baud Rate:"));
	gtk_table_attach(GTK_TABLE(Table), Label, 1, 2, 0, 1, 0, 0, 10, 5);
	Label = gtk_label_new(_("Parity:"));
	gtk_table_attach(GTK_TABLE(Table), Label, 2, 3, 0, 1, 0, 0, 10, 5);

	// create the devices combo box, and add device strings
	Combo = gtk_combo_box_text_new_with_entry();

	for(i = 0; i < g_list_length(liste); i++)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), g_list_nth_data(liste, i));
	}

	// try to restore last selected port, if any
	if(config.port != NULL && config.port[0] != '\0')
	{
		GtkWidget *tmp_entry;
		tmp_entry = gtk_bin_get_child(GTK_BIN(Combo));

		gtk_entry_set_text(GTK_ENTRY(tmp_entry), config.port);

	}
	else
	{
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 0);
	}


	// clean up devices strings
	//g_list_free(liste, (GDestroyNotify)g_free); // only available in glib >= 2.28
	for(i = 0; i < g_list_length(liste); i++)
	{
		g_free(g_list_nth_data(liste, i));
	}
	g_list_free(liste);
	g_free(chaine);

	gtk_table_attach(GTK_TABLE(Table), Combo, 0, 1, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 5);
	Combos[0] = Combo;

	Combo = gtk_combo_box_text_new_with_entry();
	gtk_entry_set_max_length(GTK_ENTRY(gtk_bin_get_child (GTK_BIN (Combo))), 10);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "300");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "600");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "1200");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "2400");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "4800");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "9600");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "19200");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "38400");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "57600");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "115200");

	/* set the current choice to the previous setting */
	switch(config.vitesse)
	{
	case 300:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 0);
		break;
	case 600:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 1);
		break;
	case 1200:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 2);
		break;
	case 2400:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 3);
		break;
	case 4800:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 4);
		break;
	case 9600:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 5);
		break;
	case 19200:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 6);
		break;
	case 38400:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 7);
		break;
	case 57600:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 8);
		break;
	case 115200:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 9);
		break;
	case 0:
		/* no previous setting, use a default */
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 5);
	default:
		/* custom baudrate */
		string = g_strdup_printf("%d", config.vitesse);
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child (GTK_BIN (Combo))), string);
		g_free(string);
	}

	//validate input text (digits only)
	g_signal_connect(GTK_ENTRY(gtk_bin_get_child (GTK_BIN (Combo))),
	                 "insert-text",
	                 G_CALLBACK(check_text_input), isdigit);

	gtk_table_attach(GTK_TABLE(Table), Combo, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 5);
	Combos[1] = Combo;

	Combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "none");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "odd");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "even");

	switch(config.parite)
	{
	case 0:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 0);
		break;
	case 1:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 1);
		break;
	case 2:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 2);
		break;
	}
	gtk_table_attach(GTK_TABLE(Table), Combo, 2, 3, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 5);
	Combos[2] = Combo;

	Label = gtk_label_new(_("Bits:"));
	gtk_table_attach(GTK_TABLE(Table), Label, 0, 1, 2, 3, 0, 0, 10, 5);
	Label = gtk_label_new(_("Stopbits:"));
	gtk_table_attach(GTK_TABLE(Table), Label, 1, 2, 2, 3, 0, 0, 10, 5);
	Label = gtk_label_new(_("Flow control:"));
	gtk_table_attach(GTK_TABLE(Table), Label, 2, 3, 2, 3, 0, 0, 10, 5);

	Combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "5");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "6");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "7");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "8");
	gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 3);

	if(config.bits >= 5 && config.bits <= 8)
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), config.bits - 5);
	gtk_table_attach(GTK_TABLE(Table), Combo, 0, 1, 3, 4, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 5);
	Combos[3] = Combo;

	Combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "1");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "2");
	gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 0);

	if(config.stops == 1 || config.stops == 2)
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), config.stops - 1);
	gtk_table_attach(GTK_TABLE(Table), Combo, 1, 2, 3, 4, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 5);
	Combos[4] = Combo;

	Combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "none");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "RTS/CTS");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "Xon/Xoff");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(Combo), "RS485-HalfDuplex(RTS)");
	gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 0);

	switch(config.flux)
	{
	case 0:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 0);
		break;
	case 1:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 2);
		break;
	case 2:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 1);
		break;
	case 3:
		gtk_combo_box_set_active(GTK_COMBO_BOX(Combo), 3);
		break;
	}
	gtk_table_attach(GTK_TABLE(Table), Combo, 2, 3, 3, 4, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 5);
	Combos[5] = Combo;

	/* create an expander widget to hide the 'Advanced features' */
	Expander = gtk_expander_new_with_mnemonic(_("Advanced Configuration Options"));
	ExpanderVbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(Expander), ExpanderVbox);
	gtk_container_add(GTK_CONTAINER(content_area), Expander);

	Frame = gtk_frame_new(_("ASCII file transfer"));
	gtk_container_add(GTK_CONTAINER(ExpanderVbox), Frame);

	Table = gtk_table_new(2, 2, FALSE);
	gtk_container_add(GTK_CONTAINER(Frame), Table);

	Label = gtk_label_new(_("End of line delay (milliseconds):"));
	gtk_table_attach_defaults(GTK_TABLE(Table), Label, 0, 1, 0, 1);

	adj = gtk_adjustment_new(0.0, 0.0, 500.0, 10.0, 20.0, 0.0);
	Spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin), TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(Spin), (gfloat)config.delai);
	gtk_table_attach(GTK_TABLE(Table), Spin, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 5);
	Combos[6] = Spin;

	Entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(Entry), 1);

	gtk_widget_set_sensitive(GTK_WIDGET(Entry), FALSE);
	gtk_table_attach(GTK_TABLE(Table), Entry, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 5);

	CheckBouton = gtk_check_button_new_with_label(_("Wait for this special character before passing to next line:"));

	g_signal_connect(GTK_WIDGET(CheckBouton), "clicked", G_CALLBACK(Grise_Degrise), (gpointer)Spin);

	if(config.car != -1)
	{
		gtk_entry_set_text(GTK_ENTRY(Entry), &(config.car));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(CheckBouton), TRUE);
	}
	gtk_table_attach_defaults(GTK_TABLE(Table), CheckBouton, 0, 1, 1, 2);
	Combos[7] = CheckBouton;


	Frame = gtk_frame_new(_("RS485 half-duplex parameters (RTS signal used to send)"));

	gtk_container_add(GTK_CONTAINER(ExpanderVbox), Frame);

	Table = gtk_table_new(2, 2, FALSE);
	gtk_container_add(GTK_CONTAINER(Frame), Table);

	Label = gtk_label_new(_("Time with RTS 'on' before transmit (milliseconds):"));
	gtk_table_attach_defaults(GTK_TABLE(Table), Label, 0, 1, 0, 1);
	Label = gtk_label_new(_("Time with RTS 'on' after transmit (milliseconds):"));
	gtk_table_attach_defaults(GTK_TABLE(Table), Label, 0, 1, 1, 2);

	adj = gtk_adjustment_new(0.0, 0.0, 500.0, 10.0, 20.0, 0.0);
	Spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin), TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(Spin), (gfloat)config.rs485_rts_time_before_transmit);
	gtk_table_attach(GTK_TABLE(Table), Spin, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 5);
	Combos[8] = Spin;

	adj = gtk_adjustment_new(0.0, 0.0, 500.0, 10.0, 20.0, 0.0);
	Spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(Spin), TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(Spin), (gfloat)config.rs485_rts_time_after_transmit);
	gtk_table_attach(GTK_TABLE(Table), Spin, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 5, 5);
	Combos[9] = Spin;


	Bouton_OK = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_box_pack_start(GTK_BOX(action_area), Bouton_OK, FALSE, TRUE, 0);
	g_signal_connect(GTK_WIDGET(Bouton_OK), "clicked", G_CALLBACK(Lis_Config), (gpointer)Combos);
	g_signal_connect_swapped(GTK_WIDGET(Bouton_OK), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_WIDGET(Dialogue));
	Bouton_annule = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	g_signal_connect_swapped(GTK_WIDGET(Bouton_annule), "clicked", G_CALLBACK(gtk_widget_destroy), GTK_WIDGET(Dialogue));
	gtk_box_pack_start(GTK_BOX(action_area), Bouton_annule, FALSE, TRUE, 0);

	gtk_widget_show_all(Dialogue);
}

gint Lis_Config(GtkWidget *bouton, GtkWidget **Combos)
{
	gchar *message;

	message = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(Combos[0]));
	strcpy(config.port, message);
	g_free(message);

	message = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(Combos[1]));
	config.vitesse = atoi(message);
	g_free(message);

	message = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(Combos[3]));
	config.bits = atoi(message);
	g_free(message);

	config.delai = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(Combos[6]));
	config.rs485_rts_time_before_transmit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(Combos[8]));
	config.rs485_rts_time_after_transmit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(Combos[9]));


	message = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(Combos[2]));
	if(!strcmp(message, "odd"))
		config.parite = 1;
	else if(!strcmp(message, "even"))
		config.parite = 2;
	else
		config.parite = 0;
	g_free(message);

	message = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(Combos[4]));
	config.stops = atoi(message);
	g_free(message);

	message = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(Combos[5]));
	if(!strcmp(message, "Xon/Xoff"))
		config.flux = 1;
	else if(!strcmp(message, "RTS/CTS"))
		config.flux = 2;
	else if(!strncmp(message, "RS485",5))
		config.flux = 3;
	else
		config.flux = 0;
	g_free(message);

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Combos[7])))
	{
		config.car = *gtk_entry_get_text(GTK_ENTRY(Entry));
		config.delai = 0;
	}
	else
		config.car = -1;

	Config_port();

	message = get_port_string();
	Set_status_message(message);
	Set_window_title(message);
	g_free(message);

	return FALSE;
}

gint Grise_Degrise(GtkWidget *bouton, gpointer pointeur)
{
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(bouton)))
	{
		gtk_widget_set_sensitive(GTK_WIDGET(Entry), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(pointeur), FALSE);
	}
	else
	{
		gtk_widget_set_sensitive(GTK_WIDGET(Entry), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(pointeur), TRUE);
	}
	return FALSE;
}

void read_font_button(GtkFontButton *fontButton)
{
	g_free(term_conf.font);
	term_conf.font = g_strdup(gtk_font_button_get_font_name(fontButton));

	if(term_conf.font != NULL)
		vte_terminal_set_font(VTE_TERMINAL(display), pango_font_description_from_string(term_conf.font));
}


void select_config_callback(GtkAction *action, gpointer data)
{
	Select_config(_("Load configuration"), G_CALLBACK(load_config));
}

void save_config_callback(GtkAction *action, gpointer data)
{
	Save_config_file();
}

void delete_config_callback(GtkAction *action, gpointer data)
{
	Select_config(_("Delete configuration"), G_CALLBACK(delete_config));
}

void Select_config(gchar *title, void *callback)
{
	GtkWidget *dialog;
	GtkWidget *content_area;
	gint i, max;

	GtkWidget *Frame, *Scroll, *Liste, *Label;
	gchar *texte_label;

	GtkListStore *Modele_Liste;
	GtkTreeIter iter_Liste;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *Colonne;
	GtkTreeSelection *Selection_Liste;

	enum
	{
		N_texte,
		N_COLONNES
	};

	/* Parse the config file */

	max = cfgParse(config_file, cfg, CFG_INI);

	if(max == -1)
	{
		show_message(_("Cannot read configuration file!\n"), MSG_ERR);
		return;
	}

	else
	{
		gchar *Titre[]= {_("Configurations")};

		dialog = gtk_dialog_new_with_buttons (title,
		                                      NULL,
		                                      GTK_DIALOG_DESTROY_WITH_PARENT,
		                                      GTK_STOCK_CANCEL,
		                                      GTK_RESPONSE_NONE,
		                                      GTK_STOCK_OK,
		                                      GTK_RESPONSE_ACCEPT,
		                                      NULL);

		content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

		Modele_Liste = gtk_list_store_new(N_COLONNES, G_TYPE_STRING);

		Liste = gtk_tree_view_new_with_model(GTK_TREE_MODEL(Modele_Liste));
		gtk_tree_view_set_search_column(GTK_TREE_VIEW(Liste), N_texte);

		Selection_Liste = gtk_tree_view_get_selection(GTK_TREE_VIEW(Liste));
		gtk_tree_selection_set_mode(Selection_Liste, GTK_SELECTION_SINGLE);

		Frame = gtk_frame_new(NULL);

		Scroll = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_container_add(GTK_CONTAINER(Frame), Scroll);
		gtk_container_add(GTK_CONTAINER(Scroll), Liste);

		renderer = gtk_cell_renderer_text_new();

		g_object_set(G_OBJECT(renderer), "xalign", (gfloat)0.5, NULL);
		Colonne = gtk_tree_view_column_new_with_attributes(Titre[0], renderer, "text", 0, NULL);
		gtk_tree_view_column_set_sort_column_id(Colonne, 0);

		Label=gtk_label_new("");
		texte_label = g_strdup_printf("<span weight=\"bold\" style=\"italic\">%s</span>", Titre[0]);
		gtk_label_set_markup(GTK_LABEL(Label), texte_label);
		g_free(texte_label);
		gtk_tree_view_column_set_widget(GTK_TREE_VIEW_COLUMN(Colonne), Label);
		gtk_widget_show(Label);

		gtk_tree_view_column_set_alignment(GTK_TREE_VIEW_COLUMN(Colonne), 0.5f);
		gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(Colonne), FALSE);
		gtk_tree_view_append_column(GTK_TREE_VIEW(Liste), Colonne);


		for(i = 0; i < max; i++)
		{
			gtk_list_store_append(Modele_Liste, &iter_Liste);
			gtk_list_store_set(Modele_Liste, &iter_Liste, N_texte, cfgSectionNumberToName(i), -1);
		}

		gtk_widget_set_size_request(GTK_WIDGET(dialog), 200, 200);

		g_signal_connect(GTK_WIDGET(dialog), "response", G_CALLBACK (callback), GTK_TREE_SELECTION(Selection_Liste));
		g_signal_connect_swapped(GTK_WIDGET(dialog), "response", G_CALLBACK(gtk_widget_destroy), GTK_WIDGET(dialog));

		gtk_box_pack_start (GTK_BOX (content_area), Frame, TRUE, TRUE, 0);

		gtk_widget_show_all (dialog);
	}
}

void Save_config_file(void)
{
	GtkWidget *dialog, *content_area, *label, *box, *entry;

	dialog = gtk_dialog_new_with_buttons (_("Save configuration"),
	                                      NULL,
	                                      GTK_DIALOG_DESTROY_WITH_PARENT,
	                                      GTK_STOCK_CANCEL,
	                                      GTK_RESPONSE_NONE,
	                                      GTK_STOCK_OK,
	                                      GTK_RESPONSE_ACCEPT,
	                                      NULL);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG(dialog));

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	label = gtk_label_new(_("Configuration name: "));

	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), "default");
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), entry, TRUE, TRUE, 0);

	//validate input text (alpha-numeric only)
	g_signal_connect(GTK_ENTRY(entry),
	                 "insert-text",
	                 G_CALLBACK(check_text_input), isalnum);
	g_signal_connect(GTK_WIDGET(dialog), "response", G_CALLBACK(save_config), GTK_ENTRY(entry));
	g_signal_connect_swapped(GTK_WIDGET(dialog), "response", G_CALLBACK(gtk_widget_destroy), GTK_WIDGET(dialog));

	gtk_box_pack_start(GTK_BOX(content_area), box, TRUE, FALSE, 5);

	gtk_widget_show_all (dialog);
}

void really_save_config(GtkDialog *Fenetre, gint id, gpointer data)
{
	int max, cfg_num, i;
	gchar *string = NULL;

	cfg_num = -1;

	if(id == GTK_RESPONSE_ACCEPT)
	{
		max = cfgParse(config_file, cfg, CFG_INI);

		if(max == -1)
			return;

		for(i = 0; i < max; i++)
		{
			if(!strcmp((char *)data, cfgSectionNumberToName(i)))
				cfg_num = i;
		}

		/* not overwriting */
		if(cfg_num == -1)
		{
			max = cfgAllocForNewSection(cfg, (char *)data);
			cfg_num = max - 1;
		}
		else
		{
			if(remove_section(config_file, (char *)data) == -1)
			{
				show_message(_("Cannot overwrite section!"), MSG_ERR);
				return;
			}
			if(max == cfgParse(config_file, cfg, CFG_INI))
			{
				show_message(_("Cannot read configuration file!"), MSG_ERR);
				return;
			}
			max = cfgAllocForNewSection(cfg, (char *)data);
			cfg_num = max - 1;
		}

		Copy_configuration(cfg_num);
		cfgDump(config_file, cfg, CFG_INI, max);

		string = g_strdup_printf(_("Configuration [%s] saved\n"), (char *)data);
		show_message(string, MSG_WRN);
		g_free(string);
	}
	else
		Save_config_file();
}

void save_config(GtkDialog *Fenetre, gint id, GtkWidget *edit)
{
	int max, i;
	const gchar *config_name;

	if(id == GTK_RESPONSE_ACCEPT)
	{
		max = cfgParse(config_file, cfg, CFG_INI);

		if(max == -1)
			return;

		config_name = gtk_entry_get_text(GTK_ENTRY(edit));

		for(i = 0; i < max; i++)
		{
			if(!strcmp(config_name, cfgSectionNumberToName(i)))
			{
				GtkWidget *message_dialog;
				message_dialog = gtk_message_dialog_new_with_markup(GTK_WINDOW(Fenetre),
				                 GTK_DIALOG_DESTROY_WITH_PARENT,
				                 GTK_MESSAGE_QUESTION,
				                 GTK_BUTTONS_NONE,
				                 _("<b>Section [%s] already exists.</b>\n\nDo you want to overwrite it ?"),
				                 config_name);

				gtk_dialog_add_buttons(GTK_DIALOG(message_dialog),
				                       GTK_STOCK_CANCEL,
				                       GTK_RESPONSE_NONE,
				                       GTK_STOCK_YES,
				                       GTK_RESPONSE_ACCEPT,
				                       NULL);

				if (gtk_dialog_run(GTK_DIALOG(message_dialog)) == GTK_RESPONSE_ACCEPT)
					really_save_config(Fenetre, GTK_RESPONSE_ACCEPT, (gpointer)config_name);

				gtk_widget_destroy(message_dialog);

				i = max + 1;
			}
		}
		if(i == max) /* Section does not exist */
			really_save_config(Fenetre, GTK_RESPONSE_ACCEPT, (gpointer)config_name);
	}
}

void load_config(GtkDialog *Fenetre, gint id, GtkTreeSelection *Selection_Liste)
{
	GtkTreeIter iter;
	GtkTreeModel *Modele;
	gchar *txt, *message;

	if(id == GTK_RESPONSE_ACCEPT)
	{
		if(gtk_tree_selection_get_selected(Selection_Liste, &Modele, &iter))
		{
			gtk_tree_model_get(GTK_TREE_MODEL(Modele), &iter, 0, (gint *)&txt, -1);
			Load_configuration_from_file(txt);
			Verify_configuration();
			Config_port();
			add_shortcuts();

			message = get_port_string();
			Set_status_message(message);
			Set_window_title(message);
			g_free(message);
		}
	}
}

void delete_config(GtkDialog *Fenetre, gint id, GtkTreeSelection *Selection_Liste)
{
	GtkTreeIter iter;
	GtkTreeModel *Modele;
	gchar *txt;

	if(id == GTK_RESPONSE_ACCEPT)
	{
		if(gtk_tree_selection_get_selected(Selection_Liste, &Modele, &iter))
		{
			gtk_tree_model_get(GTK_TREE_MODEL(Modele), &iter, 0, (gint *)&txt, -1);
			if(remove_section(config_file, txt) == -1)
				show_message(_("Cannot delete section!"), MSG_ERR);
		}
	}
}

gint Load_configuration_from_file(gchar *config_name)
{
	int max, i, j, k, size;
	gchar *string = NULL;
	gchar *str;
	macro_t *macros = NULL;
	cfgList *t;

	max = cfgParse(config_file, cfg, CFG_INI);

	if(max == -1)
		return -1;

	else
	{
		for(i = 0; i < max; i++)
		{
			if(!strcmp(config_name, cfgSectionNumberToName(i)))
			{
				Hard_default_configuration();

				if(port[i] != NULL)
					strcpy(config.port, port[i]);
				if(speed[i] != 0)
					config.vitesse = speed[i];
				if(bits[i] != 0)
					config.bits = bits[i];
				if(stopbits[i] != 0)
					config.stops = stopbits[i];
				if(parity[i] != NULL)
				{
					if(!g_ascii_strcasecmp(parity[i], "none"))
						config.parite = 0;
					else if(!g_ascii_strcasecmp(parity[i], "odd"))
						config.parite = 1;
					else if(!g_ascii_strcasecmp(parity[i], "even"))
						config.parite = 2;
				}
				if(flow[i] != NULL)
				{
					if(!g_ascii_strcasecmp(flow[i], "none"))
						config.flux = 0;
					else if(!g_ascii_strcasecmp(flow[i], "xon"))
						config.flux = 1;
					else if(!g_ascii_strcasecmp(flow[i], "rts"))
						config.flux = 2;
					else if(!g_ascii_strcasecmp(flow[i], "rs485"))
						config.flux = 3;
				}

				config.delai = wait_delay[i];

				if(wait_char[i] != 0)
					config.car = (signed char)wait_char[i];
				else
					config.car = -1;

				config.rs485_rts_time_before_transmit = rts_time_before_tx[i];
				config.rs485_rts_time_after_transmit = rts_time_after_tx[i];

				if(echo[i] != -1)
					config.echo = (gboolean)echo[i];
				else
					config.echo = FALSE;

				if(crlfauto[i] != -1)
					config.crlfauto = (gboolean)crlfauto[i];
				else
					config.crlfauto = FALSE;

				g_free(term_conf.font);
				term_conf.font = g_strdup(font[i]);

				t = macro_list[i];
				size = 0;
				if(t != NULL)
				{
					size++;
					while(t->next != NULL)
					{
						t = t->next;
						size++;
					}
				}

				if(size != 0)
				{
					t = macro_list[i];
					macros = g_malloc(size * sizeof(macro_t));
					if(macros == NULL)
					{
						perror("malloc");
						return -1;
					}
					for(j = 0; j < size; j++)
					{
						for(k = 0; k < (strlen(t->str) - 1); k++)
						{
							if((t->str[k] == ':') && (t->str[k + 1] == ':'))
								break;
						}
						macros[j].shortcut = g_strndup(t->str, k);
						str = &(t->str[k + 2]);
						macros[j].action = g_strdup(str);

						t = t->next;
					}
				}

				remove_shortcuts();
				create_shortcuts(macros, size);
				g_free(macros);

				if(show_cursor[i] != -1)
					term_conf.show_cursor = (gboolean)show_cursor[i];
				else
					term_conf.show_cursor = FALSE;

				if(rows[i] != 0)
					term_conf.rows = rows[i];

				if(columns[i] != 0)
					term_conf.columns = columns[i];

				if(scrollback[i] != 0)
					term_conf.scrollback = scrollback[i];

				if(visual_bell[i] != -1)
					term_conf.visual_bell = (gboolean)visual_bell[i];
				else
					term_conf.visual_bell = FALSE;

				term_conf.foreground_color.red = foreground_red[i];
				term_conf.foreground_color.green = foreground_green[i];
				term_conf.foreground_color.blue = foreground_blue[i];
				term_conf.foreground_color.alpha = foreground_alpha[i];

				term_conf.background_color.red = background_red[i];
				term_conf.background_color.green = background_green[i];
				term_conf.background_color.blue = background_blue[i];
				term_conf.background_color.alpha = background_alpha[i];

				/* rows and columns are empty when the conf is autogenerate in the
				   first save; so set term to default */
				if(rows[i] == 0 || columns[i] == 0)
				{
					term_conf.show_cursor = TRUE;
					term_conf.rows = 80;
					term_conf.columns = 25;
					term_conf.scrollback = DEFAULT_SCROLLBACK;
					term_conf.visual_bell = FALSE;

					term_conf.foreground_color.red = 0.66;
					term_conf.foreground_color.green = 0.66;
					term_conf.foreground_color.blue = 0.66;
					term_conf.foreground_color.alpha = 1;

					term_conf.background_color.red = 0;
					term_conf.background_color.green = 0;
					term_conf.background_color.blue = 0;
					term_conf.background_color.alpha = 1;
				}

				i = max + 1;
			}
		}
		if(i == max)
		{
			string = g_strdup_printf(_("No section \"%s\" in configuration file\n"), config_name);
			show_message(string, MSG_ERR);
			g_free(string);
			return -1;
		}
	}

	vte_terminal_set_font(VTE_TERMINAL(display), pango_font_description_from_string(term_conf.font));

	vte_terminal_set_size (VTE_TERMINAL(display), term_conf.rows, term_conf.columns);
	vte_terminal_set_scrollback_lines (VTE_TERMINAL(display), term_conf.scrollback);
	vte_terminal_set_color_foreground (VTE_TERMINAL(display), &term_conf.foreground_color);
	vte_terminal_set_color_background (VTE_TERMINAL(display), &term_conf.background_color);
	gtk_widget_queue_draw(display);

	return 0;
}

void Verify_configuration(void)
{
	gchar *string = NULL;

	switch(config.vitesse)
	{
	case 300:
	case 600:
	case 1200:
	case 2400:
	case 4800:
	case 9600:
	case 19200:
	case 38400:
	case 57600:
	case 115200:
		break;

	default:
		string = g_strdup_printf(_("Baudrate %d may not be supported by all hardware"), config.vitesse);
		show_message(string, MSG_ERR);
		g_free(string);
	}

	if(config.stops != 1 && config.stops != 2)
	{
		string = g_strdup_printf(_("Invalid number of stop-bits: %d\nFalling back to default number of stop-bits number: %d\n"), config.stops, DEFAULT_STOP);
		show_message(string, MSG_ERR);
		config.stops = DEFAULT_STOP;
		g_free(string);
	}

	if(config.bits < 5 || config.bits > 8)
	{
		string = g_strdup_printf(_("Invalid number of bits: %d\nFalling back to default number of bits: %d\n"), config.bits, DEFAULT_BITS);
		show_message(string, MSG_ERR);
		config.bits = DEFAULT_BITS;
		g_free(string);
	}

	if(config.delai < 0 || config.delai > 500)
	{
		string = g_strdup_printf(_("Invalid delay: %d ms\nFalling back to default delay: %d ms\n"), config.delai, DEFAULT_DELAY);
		show_message(string, MSG_ERR);
		config.delai = DEFAULT_DELAY;
		g_free(string);
	}

	if(term_conf.font == NULL)
		term_conf.font = g_strdup_printf(DEFAULT_FONT);

}

gint Check_configuration_file(void)
{
	struct stat my_stat;
	gchar *string = NULL;

	/* is configuration file present ? */
	if(stat(config_file, &my_stat) == 0)
	{
		/* If bad configuration file, fallback to _hardcoded_ defaults! */
		if(Load_configuration_from_file("default") == -1)
		{
			Hard_default_configuration();
			return -1;
		}
	}

	/* if not, create it, with the [default] section */
	else
	{
		string = g_strdup_printf(_("Configuration file (%s) with\n[default] configuration has been created.\n"), config_file);
		show_message(string, MSG_WRN);
		cfgAllocForNewSection(cfg, "default");
		Hard_default_configuration();
		Copy_configuration(0);
		cfgDump(config_file, cfg, CFG_INI, 1);
		g_free(string);
	}
	return 0;
}

void Hard_default_configuration(void)
{
	strcpy(config.port, DEFAULT_PORT);
	config.vitesse = DEFAULT_SPEED;
	config.parite = DEFAULT_PARITY;
	config.bits = DEFAULT_BITS;
	config.stops = DEFAULT_STOP;
	config.flux = DEFAULT_FLOW;
	config.delai = DEFAULT_DELAY;
	config.rs485_rts_time_before_transmit = DEFAULT_DELAY_RS485;
	config.rs485_rts_time_after_transmit = DEFAULT_DELAY_RS485;
	config.car = DEFAULT_CHAR;
	config.echo = DEFAULT_ECHO;
	config.crlfauto = FALSE;

	term_conf.font = g_strdup_printf(DEFAULT_FONT);

	term_conf.show_cursor = TRUE;
	term_conf.rows = 80;
	term_conf.columns = 25;
	term_conf.scrollback = DEFAULT_SCROLLBACK;
	term_conf.visual_bell = TRUE;

	Selec_couleur(&term_conf.foreground_color, 0.66, 0.66, 0.66, 1.0);
	Selec_couleur(&term_conf.background_color, 0, 0, 0, 1.0);
}

void Copy_configuration(int pos)
{
	gchar *string = NULL;
	macro_t *macros = NULL;
	gint size, i;

	string = g_strdup(config.port);
	cfgStoreValue(cfg, "port", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", config.vitesse);
	cfgStoreValue(cfg, "speed", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", config.bits);
	cfgStoreValue(cfg, "bits", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", config.stops);
	cfgStoreValue(cfg, "stopbits", string, CFG_INI, pos);
	g_free(string);

	switch(config.parite)
	{
	case 0:
		string = g_strdup_printf("none");
		break;
	case 1:
		string = g_strdup_printf("odd");
		break;
	case 2:
		string = g_strdup_printf("even");
		break;
	default:
		string = g_strdup_printf("none");
	}
	cfgStoreValue(cfg, "parity", string, CFG_INI, pos);
	g_free(string);

	switch(config.flux)
	{
	case 0:
		string = g_strdup_printf("none");
		break;
	case 1:
		string = g_strdup_printf("xon");
		break;
	case 2:
		string = g_strdup_printf("rts");
		break;
	case 3:
		string = g_strdup_printf("rs485");
		break;
	default:
		string = g_strdup_printf("none");
	}

	cfgStoreValue(cfg, "flow", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", config.delai);
	cfgStoreValue(cfg, "wait_delay", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", config.car);
	cfgStoreValue(cfg, "wait_char", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", config.rs485_rts_time_before_transmit);
	cfgStoreValue(cfg, "rs485_rts_time_before_tx", string, CFG_INI, pos);
	g_free(string);
	string = g_strdup_printf("%d", config.rs485_rts_time_after_transmit);
	cfgStoreValue(cfg, "rs485_rts_time_after_tx", string, CFG_INI, pos);
	g_free(string);

	if(config.echo == FALSE)
		string = g_strdup_printf("False");
	else
		string = g_strdup_printf("True");

	cfgStoreValue(cfg, "echo", string, CFG_INI, pos);
	g_free(string);

	if(config.crlfauto == FALSE)
		string = g_strdup_printf("False");
	else
		string = g_strdup_printf("True");

	cfgStoreValue(cfg, "crlfauto", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup(term_conf.font);
	cfgStoreValue(cfg, "font", string, CFG_INI, pos);
	g_free(string);

	macros = get_shortcuts(&size);
	for(i = 0; i < size; i++)
	{
		string = g_strdup_printf("%s::%s", macros[i].shortcut, macros[i].action);
		cfgStoreValue(cfg, "macros", string, CFG_INI, pos);
		g_free(string);
	}

	g_free(string);

	if(term_conf.show_cursor == FALSE)
		string = g_strdup_printf("False");
	else
		string = g_strdup_printf("True");
	cfgStoreValue(cfg, "term_show_cursor", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", term_conf.rows);
	cfgStoreValue(cfg, "term_rows", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", term_conf.columns);
	cfgStoreValue(cfg, "term_columns", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", term_conf.scrollback);
	cfgStoreValue(cfg, "term_scrollback", string, CFG_INI, pos);
	g_free(string);

	if(term_conf.visual_bell == FALSE)
		string = g_strdup_printf("False");
	else
		string = g_strdup_printf("True");
	cfgStoreValue(cfg, "term_visual_bell", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", term_conf.foreground_color.red);
	cfgStoreValue(cfg, "term_foreground_red", string, CFG_INI, pos);
	g_free(string);
	string = g_strdup_printf("%d", term_conf.foreground_color.green);
	cfgStoreValue(cfg, "term_foreground_green", string, CFG_INI, pos);
	g_free(string);
	string = g_strdup_printf("%d", term_conf.foreground_color.blue);
	cfgStoreValue(cfg, "term_foreground_blue", string, CFG_INI, pos);
	g_free(string);
	string = g_strdup_printf("%d", term_conf.foreground_color.alpha);
	cfgStoreValue(cfg, "term_foreground_alpha", string, CFG_INI, pos);
	g_free(string);

	string = g_strdup_printf("%d", term_conf.background_color.red);
	cfgStoreValue(cfg, "term_background_red", string, CFG_INI, pos);
	g_free(string);
	string = g_strdup_printf("%d", term_conf.background_color.green);
	cfgStoreValue(cfg, "term_background_green", string, CFG_INI, pos);
	g_free(string);
	string = g_strdup_printf("%d", term_conf.background_color.blue);
	cfgStoreValue(cfg, "term_background_blue", string, CFG_INI, pos);
	g_free(string);
	string = g_strdup_printf("%d", term_conf.background_color.alpha);
	cfgStoreValue(cfg, "term_background_alpha", string, CFG_INI, pos);
	g_free(string);
}


gint remove_section(gchar *cfg_file, gchar *section)
{
	FILE *f = NULL;
	char *buffer = NULL;
	char *buf;
	long size;
	gchar *to_search;
	long i, j, length, sect;

	f = fopen(cfg_file, "r");
	if(f == NULL)
	{
		perror(cfg_file);
		return -1;
	}

	fseek(f, 0L, SEEK_END);
	size = ftell(f);
	rewind(f);

	buffer = g_malloc(size);
	if(buffer == NULL)
	{
		perror("malloc");
		return -1;
	}

	if(fread(buffer, 1, size, f) != size)
	{
		perror(cfg_file);
		fclose(f);
		return -1;
	}

	to_search = g_strdup_printf("[%s]", section);
	length = strlen(to_search);

	/* Search section */
	for(i = 0; i < size - length; i++)
	{
		for(j = 0; j < length; j++)
		{
			if(to_search[j] != buffer[i + j])
				break;
		}
		if(j == length)
			break;
	}

	if(i == size - length)
	{
		i18n_printf(_("Cannot find section %s\n"), to_search);
		return -1;
	}

	sect = i;

	/* Search for next section */
	for(i = sect + length; i < size; i++)
	{
		if(buffer[i] == '[')
			break;
	}

	f = fopen(cfg_file, "w");
	if(f == NULL)
	{
		perror(cfg_file);
		return -1;
	}

	fwrite(buffer, 1, sect, f);
	buf = buffer + i;
	fwrite(buf, 1, size - i, f);
	fclose(f);

	g_free(to_search);
	g_free(buffer);

	return 0;
}


void Config_Terminal(GtkAction *action, gpointer data)
{
	GtkWidget *Dialog, *content_area, *BoiteH, *BoiteV, *Label, *Check_Bouton, *Table, *HScale, *Entry;
	gchar *font, *scrollback;
	GtkWidget *fontButton;
	GtkWidget *fg_color_button;
	GtkWidget *bg_color_button;

	Dialog = gtk_dialog_new_with_buttons (_("Terminal configuration"),
	                                      NULL,
	                                      GTK_DIALOG_DESTROY_WITH_PARENT,
	                                      GTK_STOCK_CLOSE,
	                                      GTK_RESPONSE_CLOSE,
	                                      NULL);
	gtk_widget_set_size_request(GTK_WIDGET(Dialog), 400, 400);


	BoiteV = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_set_border_width(GTK_CONTAINER(BoiteV), 10);

	BoiteH = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	Label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(Label), "<b>Font selection: </b>");
	gtk_box_pack_start(GTK_BOX(BoiteH), Label, FALSE, TRUE, 0);

	font =  g_strdup (term_conf.font);
	fontButton = gtk_font_button_new_with_font(font);
	gtk_box_pack_start(GTK_BOX(BoiteH), fontButton, FALSE, TRUE, 10);
	g_signal_connect(GTK_WIDGET(fontButton), "font-set", G_CALLBACK(read_font_button), 0);
	gtk_box_pack_start(GTK_BOX(BoiteV), BoiteH, FALSE, TRUE, 0);

	Check_Bouton = gtk_check_button_new_with_label("Show cursor");
	g_signal_connect(GTK_WIDGET(Check_Bouton), "toggled", G_CALLBACK(Curseur_OnOff), 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Check_Bouton), term_conf.show_cursor);
	gtk_box_pack_start(GTK_BOX(BoiteV), Check_Bouton, FALSE, TRUE, 5);

	Label = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(Label), 0, 0);
	gtk_label_set_markup(GTK_LABEL(Label), "<b>Colors: </b>");
	gtk_box_pack_start(GTK_BOX(BoiteV), Label, FALSE, TRUE, 10);


	Table = gtk_table_new(2, 2, FALSE);

	Label = gtk_label_new("Text color:");
	gtk_misc_set_alignment(GTK_MISC(Label), 0, 0);
	gtk_table_attach(GTK_TABLE(Table), Label, 0, 1, 0, 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 10, 0);

	Label = gtk_label_new("Background color:");
	gtk_misc_set_alignment(GTK_MISC(Label), 0, 0);
	gtk_table_attach(GTK_TABLE(Table), Label, 0, 1, 1, 2, GTK_SHRINK | GTK_FILL , GTK_SHRINK, 10, 0);

	fg_color_button = gtk_color_button_new_with_rgba (&term_conf.foreground_color);
	gtk_color_button_set_title (GTK_COLOR_BUTTON (fg_color_button), _("Text color"));
	gtk_table_attach (GTK_TABLE (Table), fg_color_button, 1, 2, 0, 1, GTK_SHRINK, GTK_SHRINK, 10, 0);
	g_signal_connect (GTK_WIDGET (fg_color_button), "color-set", G_CALLBACK (config_fg_color), NULL);

	bg_color_button = gtk_color_button_new_with_rgba (&term_conf.background_color);
	gtk_color_button_set_title (GTK_COLOR_BUTTON (bg_color_button), _("Background color"));
	gtk_table_attach (GTK_TABLE (Table), bg_color_button, 1, 2, 1, 2, GTK_SHRINK, GTK_SHRINK, 10, 0);
	g_signal_connect (GTK_WIDGET (bg_color_button), "color-set", G_CALLBACK (config_bg_color), NULL);

	gtk_box_pack_start(GTK_BOX(BoiteV), Table, FALSE, TRUE, 0);


	gtk_box_pack_start(GTK_BOX(BoiteV), Label, FALSE, TRUE, 10);

	Label = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(Label), 0, 0);
	gtk_label_set_markup(GTK_LABEL(Label), "<b>Screen: </b>");
	gtk_box_pack_start(GTK_BOX(BoiteV), Label, FALSE, TRUE, 10);

	BoiteH = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	Label = gtk_label_new("Scrollback lines:");
	gtk_box_pack_start(GTK_BOX(BoiteH), Label, FALSE, TRUE, 0);
	Entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(Entry), 4);
	scrollback =  g_strdup_printf("%d", term_conf.scrollback);
	gtk_entry_set_text(GTK_ENTRY(Entry), scrollback);
	g_free(scrollback);
	g_signal_connect(GTK_WIDGET(Entry), "focus-out-event", G_CALLBACK(scrollback_set), 0);
	gtk_box_pack_start(GTK_BOX(BoiteH), Entry, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(BoiteV), BoiteH, FALSE, TRUE, 0);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG(Dialog));
	gtk_box_pack_start(GTK_BOX(content_area), BoiteV, FALSE, TRUE, 0);

	g_signal_connect_swapped(GTK_WIDGET(Dialog), "response", G_CALLBACK(gtk_widget_destroy), GTK_WIDGET(Dialog));

	gtk_widget_show_all (Dialog);
}

void Curseur_OnOff(GtkWidget *Check_Bouton, gpointer data)
{
	term_conf.show_cursor = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Check_Bouton));
}

void Selec_couleur(GdkRGBA *color, gfloat R, gfloat G, gfloat B, gfloat A)
{
	color->red = R;
	color->green = G;
	color->blue = B;
	color->alpha = A;
}

void config_fg_color(GtkWidget *button, gpointer data)
{
	gchar *string;

	gtk_color_button_get_rgba (GTK_COLOR_BUTTON (button), &term_conf.foreground_color);

	vte_terminal_set_color_foreground (VTE_TERMINAL(display), &term_conf.foreground_color);
	gtk_widget_queue_draw (display);

	string = g_strdup_printf ("%d", term_conf.foreground_color.red);
	cfgStoreValue (cfg, "term_foreground_red", string, CFG_INI, 0);
	g_free (string);
	string = g_strdup_printf ("%d", term_conf.foreground_color.green);
	cfgStoreValue (cfg, "term_foreground_green", string, CFG_INI, 0);
	g_free (string);
	string = g_strdup_printf ("%d", term_conf.foreground_color.blue);
	cfgStoreValue (cfg, "term_foreground_blue", string, CFG_INI, 0);
	g_free (string);
	string = g_strdup_printf ("%d", term_conf.foreground_color.alpha);
	cfgStoreValue (cfg, "term_foreground_alpha", string, CFG_INI, 0);
	g_free (string);
}

void config_bg_color(GtkWidget *button, gpointer data)
{
	printf("config_bg_color\r\n");
	gchar *string;

	gtk_color_button_get_rgba (GTK_COLOR_BUTTON (button), &term_conf.background_color);

	vte_terminal_set_color_background (VTE_TERMINAL(display), &term_conf.background_color);
	gtk_widget_queue_draw (display);

	string = g_strdup_printf ("%d", term_conf.background_color.red);
	cfgStoreValue (cfg, "term_background_red", string, CFG_INI, 0);
	g_free (string);
	string = g_strdup_printf ("%d", term_conf.background_color.green);
	cfgStoreValue (cfg, "term_background_green", string, CFG_INI, 0);
	g_free (string);
	string = g_strdup_printf ("%d", term_conf.background_color.blue);
	cfgStoreValue (cfg, "term_background_blue", string, CFG_INI, 0);
	g_free (string);
	string = g_strdup_printf ("%d", term_conf.background_color.alpha);
	cfgStoreValue (cfg, "term_background_alpha", string, CFG_INI, 0);
	g_free (string);
}

gint scrollback_set(GtkWidget *Entry, GdkEventFocus *event, gpointer data)
{
	const gchar *text;
	gint scrollback;

	if (Entry)
	{
		text = gtk_entry_get_text(GTK_ENTRY(Entry));
		scrollback = (gint)g_ascii_strtoll(text, NULL, 10);
		if (scrollback)
			term_conf.scrollback = scrollback;
		else
			term_conf.scrollback = DEFAULT_SCROLLBACK;
		vte_terminal_set_scrollback_lines (VTE_TERMINAL(display), term_conf.scrollback);
	}
	return FALSE;
}

/**
 *  Filter user data entry on a GTK entry
 *
 *  user_data must be a function that takes an int and returns an int
 *  != 0 if the input is valid.  For instance, 'isdigit()'.
 */
void check_text_input(GtkEditable *editable,
                      gchar       *new_text,
                      gint         new_text_length,
                      gint        *position,
                      gpointer     user_data)
{
	int i;
	int (*check_func)(int) = NULL;

	if(user_data == NULL)
	{
		return;
	}

	g_signal_handlers_block_by_func(editable,
	                                (gpointer)check_text_input, user_data);
	check_func = (int (*)(int))user_data;

	for(i = 0; i < new_text_length; i++)
	{
		if(!check_func(new_text[i]))
			goto invalid_input;
	}

	gtk_editable_insert_text(editable, new_text, new_text_length, position);

invalid_input:
	g_signal_handlers_unblock_by_func(editable,
	                                  (gpointer)check_text_input, user_data);
	g_signal_stop_emission_by_name(editable, "insert-text");
}

