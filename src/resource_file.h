/***********************************************************************
 * resource_file.h                                              
 * ---------------                          
 *           GTKTerm Software                                   
 *                      (c) Julien Schmitt               
 *                                                               
 * ------------------------------------------------------------------- 
 *                                                            
 *   \brief Purpose                                        
 *      Load and save configuration file                              
 *      - Header file -                                       
 *                                                           
 ***********************************************************************/

#ifndef RESOURCE_FILE_H_
#define RESOURCE_FILE_H_

void config_file_init (void);
void save_configuration_to_file(GKeyFile *, const char *);
int load_configuration_from_file(const char *);
int check_configuration_file ();
void dump_configuration_to_cli(char *);
void hard_default_configuration (void);
void validate_configuration(void);
void copy_configuration(GKeyFile *, const char *);
int remove_section(char *cfg_file, char *section);
void set_color(GdkRGBA *color, float, float, float, float);

extern GFile *config_file;
#endif