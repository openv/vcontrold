#!/bin/sh
aclocal
autoconf
touch NEWS README AUTHORS ChangeLog
automake --add-missing
# ./configure
# make
