# GTKTerm: The source code architecture

This file describes the architecture of gtkterm.
GtkTerm has several objects and uses signals to communicate between these 
objects.

## General description

GTKTerm is build with the GTK4 framework. It uses Gobjects and communicates 
(mostly) through signals.

GTKTerm is the main application object. It is a holder for the keyfile.
The commandline interfaces uses the application object framework to handle
all commandline options. The options are connected to the relevant GObjects by
signals.

The core of the application is the terminal. This is a VTE object and 
handles all communication to and from the serial port.
The terminal window holds the configuration of the terminal window and 
the serial ports.
The configuraton is copied from the GTKTerm application which holds the 
keyfile. It is copied back to the keyfile if it is saved.
For now the GTKTerm application has just one terminal window. The architecture
of GTKTerm is able to support multiple terminal windows in future releases.


## Objects

This part lists an overview of all objects used in GTKTerm. For details about
implementation please use the GTKTERM.pdf which is a Doxygen generated overview
of the GTKTerm source code.

### GtkTerm

#### members
#### signals


### GtkTermWindow


### GtkTermTerminal


### GtkTermConfiguration


### GtkTermSerialPort
