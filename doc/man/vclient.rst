=========
 vclient
=========

--------------------------------------------------------
vclient — vclient tool for getting values from vcontrold
--------------------------------------------------------

:Author: Frank Nobis fn@radio-do.de,
         other contributors see `vcontrold @GitHub <https://github.com/openv/vcontrold>`__
:Copyright: GPLv3
:Manual section: 1

SYNOPSIS
========

    vclient -h <ip:port> [-c <command1,command2,...>] [-f <commandfile>] [-s <csv-file>] [-t <template-file>] [-o <outputfile>] [-x <executablescript>] [-k] [-m] [-v]

DESCRIPTION
===========

vclient is the client program to communicate with the vcontrold daemon.
It features a template mode to prepare received data for logging or database input.

OPTIONS
=======

-h, --host
    <IPv4:port> or <IPv6> of vcontrold

-p, --port
    <port> of vcontrold when using IPv6

-c, \--command <commandlist>
    comma-separated list of commands

-f, \-commandfile <commandfile>
    optional file with commands, one command per line

-s, --csvfile
    output results in CSV format

-t, \--template <templatefile>
    template file, variables get replaced by returned values

-o, \--output <outputfile>
    redirect stdout into <outputfile>

-x, \--execute <scriptfile>
    write template output to <scriptfile>, then run the generated <scriptfile>

-m, \--munin
    Munin datalogger compatible output, omits units and error details

-k, \--cacti
    Cacti datalogger compatible output, omits units and error details

-v, --verbose
    verbose mode

-4, --inet4
    IPv4 preferred

-6, --inet6
    IPv6 preferred. If no option provided, us OS defaults.

--help
    this help

TEMPLATE MODE
=============

In template mode, variables contained in the <templatefile> get
replaced with the values returned by a command issued in vclient.

Variables $1, $2, ..., $n gets replaced with 1st, 2nd, ..., nth return value.

The following variable types exist for conversion of return values:

+------------+-----------------------------------+
| variable   | function                          |
+------------+-----------------------------------+
| $1..$n     | return value converted to float   |
+------------+-----------------------------------+
| $R1..$Rn   | return value converted to text    |
+------------+-----------------------------------+
| $C1..$Cn   | issued command                    |
+------------+-----------------------------------+
| $E1..$En   | error message of command          |
+------------+-----------------------------------+

EXAMPLES
========

Preparation of a simple template for a database statement to insert some values in database:

::

    $ cat > sql.tmpl <<EOF
    INSERT INTO messwerte values (CURRENT_DATE,$1,$2);
    EOF

Calling ``vclient`` with the given template shows the variables being replaced by their values:

::

    $ vclient -h 127.0.0.1:1234 -t sql.tmpl -c gettempA,gettempWW
    INSERT INTO messwerte values (CURRENT\_DATE,-2.600000,54.299999);

These lines could be written directly into a database by piping the
output to a DB cli:

``$ vclient -h 127.0.0.1:1234 -t sql.tmpl -c gettempA,gettempWW 2>/dev/null | mysql -D vito``

The -o <outputfile> option writes (overwrites, not appends) the
output to the given file.

With the option -x <scriptfile*, the generated output is treated
like a script, which is run after output generation.

Example:

::

    $ cat > sh-example.tmpl <<EOF
    #!/bin/sh
    DB=vitodb.rrd
    echo "command 1: $C1 ; command 2: $C2"
    echo "rrdtool update $DB N:$1:$2"
    EOF

``$ vclient -h 127.0.0.1:1234 -c getTempA,getTempWW -t sh-example.tmpl -x sh-example.sh``

Output:

::

    command 1: getTempA ; command 2: getTempWW
    rrdb update vitodb.rrd N:-2.600000:54.299999

SEE ALSO
========

* man 1 vcontrold
* `vcontrold @GitHub <https://github.com/openv/vcontrold>`__
