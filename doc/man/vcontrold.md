vcontrold 1 User Manual
=======================

NAME
----

vcontrold — Unix daemon for Vito communication

SYNOPSIS
--------

`vcontrold` [`−x` *xml-file*] [`−d` *device*] [`−l` *logfile*] [`−p` *port*] [`−s`] [`−n`] [`−i`] [`−g`]

DESCRIPTION
-----------

`vcontrold` uses a serial link or an IP connection to communicate with a vito heating system.

OPTIONS
-------

The meaning of the different options is:

`−x`
  used to tell `vcontrold` where the main config file is found. See FILES for more info,

`−d`
  specifies the serial device to use. This option overrides the device entry in the *xml* file.

`−p`
  the TCP *port* to use for remote connections. The default is 3002 and can be specified in the *xml* file. This option overrides the corresponding option int the *xml* file.

`−s`
  use syslog

`−l`
  use *logfile* instead of syslog.

`−n`
  do not fork. This is for testing purpose only. Normaly `vcontrold` will detach from the controlling terminal and put itself into the background.

`−i`
  use a temp file in /tmp/sim-devid.ini for use with the vsim simulator

`−g`
  enable debug mode.

FILES
-----

*/etc/vcontrold/vcontrold.xml*
  These are the programm specific configurations. E.g. device, baudrate, IP etc.

*/etc/vcontrold/vito.xml*
  These are the command definitions for the devices in use.

AUTHOR
------

Frank Nobis <fn@radio-do.de>,
other contributors see [vcontrold @GitHub](https://github.com/openv/vcontrold)

LICENSE
-------

The `vcontrold` software, its accompanying files and documentation are licensed under **GPLv3**.
See `COPYING` in the package.

SEE ALSO
--------

vclient(1), [vcontrold @GitHub](https://github.com/openv/vcontrold)
