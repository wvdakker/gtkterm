# GTKTerm: The source code architecture

This file describes the architecture of GTKTerm.
GtkTerm has several objects and uses signals to communicate between these 
objects.

One of the subgoals is not to use any global variables but exchange data
by the use of signals. For that only the array of signals is a global
variable.

Use of GTKTerm/GtkTerm/gtkterm naming schema:
In this document several ways of Upper/Lowercase combinations of GTKTerm is 
used:
- GTKTerm: The name of the application
- GtkTerm: The first part of the name of the object in the source code. 
For example: GtkTermWindow.
- gtk_term: The first part of the function of an object in the source code.
For example: gtkterm_window_init

## General description

GTKTerm is build with the GTK4 framework. It uses Gobjects and communicates 
(mostly) through signals.

GTKTerm is the main application object. It is a holder for the keyfile.
The commandline interfaces uses the application object framework to handle
all commandline options. The options are connected to the relevant GObjects by
signals.
Almost all objects have a 'public' and 'private' part. However the 'public' part
is not globally known (except for GtkTerm application object).

The core of the application is the terminal. This is a VTE object and 
handles all communication to and from the serial port.
The terminal window holds the configuration of the terminal window and 
the serial ports.
The configuraton is copied from the GtkTerm application which holds the 
keyfile. It is copied back to the keyfile if it is saved.
For now the GtkTerm application has just one terminal window. The architecture
of GTKTerm is able to support multiple terminal windows in future releases.

## Objects

This part lists an overview of all objects used in GTKTerm. For details about
implementation please use the GTKTERM.pdf which is a Doxygen generated overview
of the GTKTerm source code.

### GtkTerm

GtkTerm is the main GtkApplication object for GTKTerm. It starts the gtkterm_window
and handles the cmdline interface (CLI). Options given at the CLI are directly 
stored into the in memory keyfile. 
This in memory keyfile is the base for the configuration of the terminal windows. 
Getting configuration for the terminal window is done by signals for the [section] 
needed.

#### Members
#### Signals
#### Main functions

### GtkTermWindow

GtkTermWindow is the main application window for GtkTerm. It creates all widgets
and does the handling for the statusbar.

#### Members
#### Signals
#### Main functions

### GtkTermTerminal

The terminal window in which all serial communication is shown. It is an VTE object
and hold the configuration for the terminal and serial port.

#### Members
#### Signals
#### Main functions

### GtkTermConfiguration

GtkTerm does all operation on the keyfile. It loads, saves the file and removes, checks
sections.
It also copies the section configuration info the configuration for the terminal. 

#### Members
#### Signals
#### Main functions

### GtkTermSerialPort

The Serial port object which does all communication to the serial port.
It configures the port based on the port_conf from terminal.

#### Members
#### Signals
#### Main functions

## Resources

For the migration to gtk4 several links were used:
- https://docs.gtk.org/gobject/tutorial.html
- https://docs.gtk.org/gobject/concepts.html
- https://docs.gtk.org/glib/
- https://toshiocp.github.io/Gtk4-tutorial/index.html
- https://c-gtk.org/gapplication-part-i

IRC #gtk was a big support when asking questions.

Also special thanks to Jens Georg. Sellerie (an earlier fork of GTKTerm)
was used as inspiration to solve some problems.
