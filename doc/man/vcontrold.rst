===========
 vcontrold
===========

----------------------------------------------------------
Unix daemon for communication with Viessmann Vito heatings
----------------------------------------------------------

:Author: Frank Nobis fn@radio-do.de,
         other contributors see `vcontrold @GitHub <https://github.com/openv/vcontrold>`__
:Copyright: GPLv3
:Manual section: 1

SYNOPSIS
========

  vcontrold [-x <xml-file>] [-d <device>] [-l <logfile>] [-p <port>] [-s] [-n] [-i] [-g]

DESCRIPTION
===========

vcontrold uses a serial optical link or an IP connection to communicate with
a Viessmann vito heating controller.

OPTIONS
=======

-x <xml-file>
    location of the main config file

-d <serial-device>
    serial device to use.
    This option overrides the device entry
    in the <xml-file> file.

-p <port>
    TCP <port> to use for remote connections.
    The default is 3002 and can be specified
    in the <xml-file> file.
    This option overrides the corresponding option in the <xml-file> file.

-s
    use syslog

-l
    use <logfile> instead of syslog.

-n
    do not fork. This is for testing purpose only. Normaly vcontrold
    will detach from the controlling terminal and put itself into the
    background.

-i
    use a temp file in ``/tmp/sim-devid.ini`` for use with the vsim simulator

-g
    enable debug mode.

FILES
=====

``/etc/vcontrold/vcontrold.xml``
    These are the programm specific configurations. E.g. device, baudrate,
    IP etc.

``/etc/vcontrold/vito.xml``
    These are the command definitions for the devices in use.

LICENSE
=======

The vcontrold software, its accompanying files and documentation
are licensed under the **GPLv3**.
See COPYING in the package.

SEE ALSO
========

* man 1 vclient
* vcontrold @GitHub: `https://github.com/openv/vcontrold <https://github.com/openv/vcontrold>`__
