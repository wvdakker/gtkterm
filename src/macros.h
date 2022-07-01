/***********************************************************************
 * macros.h                                                            
 * --------                                                            
 *           GTKTerm Software                                          
 *                      (c) Julien Schmitt                             
 *                                                                     
 * ------------------------------------------------------------------- 
 *                                                                     
 * \brief Purpose                                                    
 *        Functions for the management of the macros                   
 *        - Header file -                                               
 *                                                                     
 ***********************************************************************/

#ifndef MACROS_H_
#define MACROS_H_

//! Define macro structure type
typedef struct
{
	char *shortcut;		//! Shortcut of the macro
	char *action;		//! Command to perform
	GClosure *closure;	//!
}
macro_t;

//void config_macros(GtkAction *action, gpointer data);
void remove_shortcuts(void); 				//! Remove shortcuts from accel_group and free memory
void add_shortcuts(void);					//! 
macro_t *get_shortcuts(gint *);				//!

void convert_string_to_macros (char **, int);
int convert_macros_to_string (char **);

int macro_count ();

extern macro_t *macros;

#endif
