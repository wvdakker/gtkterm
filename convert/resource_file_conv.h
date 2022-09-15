/************************************************************************/
/* resource_file_conv.h                                                 */
/* --------------------                                                 */
/*           GTKTerm Software                                           */
/*                      (c) Julien Schmitt                              */
/*                                                                      */
/* -------------------------------------------------------------------  */
/*                                                                      */
/*   Purpose                                                            */
/*      Conversion v1 to v2 configuration : header file                 */
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

#ifndef RESOURCE_FILE_H_
#define RESOURCE_FILE_H_

void config_file_init (void);
void save_configuration_to_file(GKeyFile *);
int load_configuration_from_file(const char *);
int check_configuration_file ();
void dump_configuration_to_cli(char *);
void hard_default_configuration (void);
void validate_configuration(void);
void copy_configuration(GKeyFile *, const char *);
int remove_section(char *cfg_file, char *section);
void set_color(GdkRGBA *color, float, float, float, float);

extern GFile *config_file;
extern void show_message (char *, int);
#endif
