Introduction:
-------------

In this directory, you will find RAW print server software for
Windows and for Linux / Unix / OS X.  For Windows, the software
is RawPrintServer.exe.  For Linux / Unix / OS X, the software
is the tiny RawPrintServer.sh script.



Windows Installation:
---------------------

For use as an operating system service (Windows NT/2000/XP):

* open a command line with "Start"-Menu Execute "cmd"
* call 'RawPrintServer INSTALL "Printer Name"' to install a RAW print server
  on port 9100 that routes data to the named printer (put printer name in
  quotation marks, and use the same name as in your Printers control panel)
* call 'RawPrintServer REMOVE' to deinstall it (might need a restart)
* if anything unclear, ask your system administrator...

For use in Windows 95/98/ME, you need standalone mode:
  RawPrintServer STANDALONE "my printer"
To terminate, press ctrl-c in the RawPrintServer window.  To get the
console to disappear, instead of STANDALONE, do BACKGROUND:
  RawPrintServer BACKGROUND "my printer"
After that, to terminate the server, you need to press ctrl-alt-delete
and terminate the RawPrintServer process.  To set RawPrintServer to start
automatically after a reboot, you might want to add it to your 
autoexec.bat file or edit your registry.  This is left to advanced users
to figure out.


You can install the print server for several different printers by using
different port numbers, usually from among 9100, 9101, 9102, ..., 9109.
To do that, add the port number at the end of the command-line, for instance:
  RawPrintServer INSTALL "my second printer" 9101
  RawPrintServer REMOVE 9101

If a port is not specified, 9100 is used.


To print to the printer, configure your printer on your client PC
to use a custom TCP/IP port in RAW (JetDirect) protocol mode, with
the specified port and the IP address (or domain name) of your
server PC.




OS X / Unix / Linux Installation
--------------------------------

Make sure you have nc (netcat) installed (search the web for it if
you need a copy).  To run the server, go to the terminal / xterm /
etc., navigate to this directory.  Make sure that RawPrintServer.sh
is executable:
   chmod 755 RawPrintServer.sh
Now, run
   ./RawPrintServer.sh printerName &
This installs the print server as a RAW print server on port 9100,
using printerName.  The printerName is the name of the printer as
would be used in an lpr -P printerName commandline.

Make sure that you don't have any firewall blocking incoming port
9100 connections.

Ask your local OS X / Unix / Linux guru if you want to set
RawPrintServer.sh to start every time the machine boots.

If you want to use a port other than 9100, edit RawPrintServer.sh
with a text editor.


Henk Jonas - metaview at web dot de
www.metaviewsoft.de
Berlin, Germany 07/2006


Alexander Pruss - arpruss at gmail dot com
Washington, DC, USA
