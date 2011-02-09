/***********************************************************************/
/* cmdline.c                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Reads the command line                                         */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.98.3 : modified for configuration file                     */
/*      - 0.98.2 : added --echo                                        */
/*      - 0.98 : file creation by Julien                               */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include "term_config.h"
#include "fichier.h"
#include "auto_config.h"
#include "i18n.h"

#include <config.h>
#include <glib/gi18n.h>

extern struct configuration_port config;

void display_help(void)
{
  i18n_printf(_("\nGTKTerm version %s\n"), VERSION);
  i18n_printf(_("\t (c) Julien Schmitt\n"));
  i18n_printf(_("\nThis program is released under the terms of the GPL V.2\n"));
  i18n_printf(_("\t ** Use at your own risks ! **\n"));
  i18n_printf(_("\nCommand line options\n"));
  i18n_printf(_("--help or -h : this help screen\n"));
  i18n_printf(_("--config <configuration> or -c : load configuration\n"));
  i18n_printf(_("--port <device> or -p : serial port device (default /dev/ttyS0)\n"));
  i18n_printf(_("--speed <speed> or -s : serial port speed (default 9600)\n"));
  i18n_printf(_("--bits <bits> or -b : number of bits (default 8)\n"));
  i18n_printf(_("--stopbits <stopbits> or -t : number of stopbits (default 1)\n"));
  i18n_printf(_("--parity <odd | even> or -a : partity (default none)\n"));
  i18n_printf(_("--flow <Xon | RTS | RS485> or -w : flow control (default none)\n"));
  i18n_printf(_("--delay <ms> or -d : end of line delay in ms (default none)\n"));
  i18n_printf(_("--char <char> or -r : wait for a special char at end of line (default none)\n"));
  i18n_printf(_("--file <filename> or -f : default file to send (default none)\n"));
  i18n_printf(_("--rts_time_before <ms> or -x : for rs485, time in ms before transmit with rts on\n"));
  i18n_printf(_("--rts_time_after <ms> or -y : for rs485, time in ms after transmit with rts on\n"));
  i18n_printf(_("--echo or -e : switch on local echo\n"));
  i18n_printf("\n");
}

int read_command_line(int argc, char **argv, gchar *configuration_to_read)
{
  int c;
  int option_index = 0;

  static struct option long_options[] = {
    {"speed", 1, 0, 's'},
    {"parity", 1, 0, 'a'},
    {"stopbits", 1, 0, 't'},
    {"bits", 1, 0, 'b'},
    {"file", 1, 0, 'f'},
    {"port", 1, 0, 'p'},
    {"flow", 1, 0, 'w'},
    {"delay", 1, 0, 'd'},
    {"char", 1, 0, 'r'},
    {"help", 0, 0, 'h'},
    {"echo", 0, 0, 'e'},
    {"rts_time_before", 1, 0, 'x'},
    {"rts_time_after", 1, 0, 'y'},
    {"config", 1, 0, 'c'},
    {0, 0, 0, 0}
  };

  /* need a working configuration file ! */
  Check_configuration_file();

  while(1) {
    c = getopt_long (argc, argv, "s:a:t:b:f:p:w:d:r:hec:x:y:", long_options, &option_index);

    if(c == -1)
      break;

    switch(c)
      {
      case 'c':
	Load_configuration_from_file(optarg);
	break;

      case 's':
	config.vitesse = atoi(optarg);
	break;

      case 'a':
	if(!strcmp(optarg, "odd"))
	  config.parite = 1;
	else if(!strcmp(optarg, "even"))
	  config.parite = 2;
	break;

      case 't':
	config.stops = atoi(optarg);
	break;

      case 'b':
	config.bits = atoi(optarg);
	break;

      case 'f':
	fic_defaut = g_strdup(optarg);
	break;

      case 'p':
	strcpy(config.port, optarg);
	break;

      case 'w':
	if(!strcmp(optarg, "Xon"))
	  config.flux = 1;
	else if(!strcmp(optarg, "RTS"))
	  config.flux = 2;
	else if(!strcmp(optarg, "RS485"))
	  config.flux = 3;
	break;

      case 'd':
	config.delai = atoi(optarg);
	break;

      case 'r':
	config.car = *optarg;
	break;

      case 'e':
	config.echo = TRUE;
	break;

      case 'x':
	config.rs485_rts_time_before_transmit = atoi(optarg);
	break;

      case 'y':
	config.rs485_rts_time_after_transmit = atoi(optarg);
	break;

      case 'h':
	display_help();
	return -1;

      default:
	i18n_printf(_("Misunderstood command line option\n"));
	return -1;
    }
  }
  Verify_configuration();
  return 0;
}

