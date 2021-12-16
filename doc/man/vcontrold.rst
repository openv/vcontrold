..
    SPDX-FileCopyrightText: 2012 Frank Nobis <frank.nobis@radio-do.de>

    SPDX-License-Identifier: CC-BY-SA-4.0

=========
vcontrold
=========

----------------------------------------------------------
Unix daemon for communication with Viessmann Vito heatings
----------------------------------------------------------

:Manual section: 1

SYNOPSIS
========

  vcontrold [-x <xml-file>] [-d <device>] [-l <logfile>] [-p <port>] [-s] [-n]
    [-c <command-file>] [-P <pid-file>] [-U <username>] [-G <groupname>]
    [-i] [-g] [-4] [-6] [-v] [-V] [-?]

DESCRIPTION
===========

vcontrold uses a serial optical link or an IP connection to communicate with
a Viessmann vito heating controller.

OPTIONS
=======

-x <xml-file>, \--xmlfile <xml-file>
    location of the main config file

-d <device>, \--device <device>
    serial device to use.
    This option overrides corresponding entry in the config file.

-l <logfile>, \--logfile <logfile>
    use <logfile> instead of syslog.

-p <port>, \--port <port>
    TCP <port> to use for remote connections.
    The default is 3002 and can be specified in the config file.
    This option overrides the corresponding entry in the config file.

-s, \--syslog
    use syslog

-n, \--nodaemon
    do not fork. This is for testing purpose only. Normaly vcontrold
    will detach from the controlling terminal and put itself into the
    background.

-c <command-file>, \--commandfile <command-file>
    file with lines containing sequences of icmds (WAIT, SEND, RECV, PAUSE)
    as used in protocol definitions.
    Lines get executed in order
    (developer option)

-P <pid-file>, \--pidfile <pid-file>
    write process id to <pid-file> when started as a daemon.
    When started as root, <pid-file> is written prior to dropping privileges.
    This overrides the corresponding entry in the config file.

-U <username>, \--username <username>
    when started by root, drop privileges to user <username>
    instead of user nobody. This overrides the corresponding entry in the config file.
    If using a serial link, ensure that user or group has access rights to the serial device.

-G <groupname>, \--groupname <groupname>
    when started by root, drop privileges to group <groupname>
    instead of group dialout. This overrides the corresponding entry in the config file.
    If using a serial link, ensure that user or group has access rights to the serial device.

-i, \--vsim
    use a temp file in ``/tmp/sim-devid.ini`` for use with the vsim simulator
    (developer option)

-g, \--debug
    enable debug mode

-4, --inet4
    use IP v4 socket

-6, --inet6
    use IP v6 socket

-v, \--verbose
    verbose mode

-V, \--Version
    print version information, then exit

-?, \--help
    usage information

FILES
=====

``/etc/vcontrold/vcontrold.xml``
    These are the programm specific configurations. E.g. device, baudrate,
    IP etc.

``/etc/vcontrold/vito.xml``
    These are the command definitions for the devices in use.

SEE ALSO
========

* man 1 vclient
* vcontrold @GitHub: `https://github.com/openv/vcontrold <https://github.com/openv/vcontrold>`__
