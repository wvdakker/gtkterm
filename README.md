# GTKTerm : a GTK+ serial port terminal
Original Code by: Julien Schmitt

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

Modifications 06/2019:
    The current version is forked from original location https://github.com/Jeija/gtkterm
    and adds a new functionality to display timestamp in logs.
    (forked project: https://github.com/ddmesh/gtkterm)

Changelog since original version:
    Version 1.1 : - fix memory access when auto CR/NL conversion is enabled
                  - fix loading and setting CR/NL configuration setting
                  - add new option to display timestamp in logs
    
- 

### Command line options

    --help or -h : this help screen
    --config <configuration> or -c : load configuration
    --port <device> or -p : serial port device (default /dev/ttyS0)
    --speed <speed> or -s : serial port speed (default 9600)
    --bits <bits> or -b : number of bits (default 8)
    --stopbits <stopbits> or -t : number of stopbits (default 1)
    --parity <odd | even> or -a : parity (default none)
    --flow <Xon | CTS> or -w : flow control (default none)
    --delay <ms> or -d : end of line delay in ms (default none)
    --char <char> or -r : wait for a special char at end of line (default none)
    --file <filename> or -f : default file to send (default none)
    --echo or -e : switch on local echo

### Keyboard shortcuts 
As Gtkterm is often used like a terminal emulator,
the shortcut keys are assigned to `<ctrl><shift>`, rather than just `<ctrl>`. This allows the user to send keystrokes of the form `<ctrl>X` and not have Gtkterm intercept them.

    <ctrl><shift>L -- Clear screen
    <ctrl><shift>R -- Send file
    <ctrl><shift>Q -- Quit 
    <ctrl><shift>S -- Configure port
    <ctrl><shift>V -- Paste
    <ctrl><shift>C -- Copy
    <ctrl>B	 -- Send break
    F5 -- Open Port
    F6 -- Close Port
    F7 -- Toggle DTR
    F8 -- Toggle RTS

#### NOTES on RS485:
The RS485 flow control is a software user-space emulation and therefore may not work for all configurations (won't respond quickly enough). If this is the case for your setup, you will need to either use a dedicated RS232 to RS485 converter, or look for a kernel level driver. This is an inherent limitation to user space programs.

### Building:
GtkTerm has a few dependencies-
* Gtk+3.0 (version 3.12 or higher)
* vte (version 0.40 or higher)
* intltool (version 0.40.0 or higher)

Once these dependencies are installed, most people should simply run:

    ./configure
    make
    #  And to install:
    make install

If you wish to install Gtkterm someplace other than the default directory, use:

    ./configure --prefix=/install/directory

Then build and install as usual.

 See `INSTALL` for more detailed build and install options.
