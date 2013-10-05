# what flags you want to pass to the C compiler & linker
AM_CFLAGS =
AM_LDFLAGS =

# this lists the binaries to produce, the (non-PHONY, binary) targets in
# the previous manual Makefile
sbin_PROGRAMS = vcontrold
bin_PROGRAMS = vclient
vcontrold_SOURCES = vcontrold.c parser.c io.c common.c socket.c xmlconfig.c arithmetic.c unit.c semaphore.c parser.h io.h common.h socket.h xmlconfig.h arithmetic.h unit.h semaphore.h
vclient_SOURCES = vclient.c client.c client.h prompt.h common.c common.h socket.c socket.h io.c io.h
# dist_sysconf_DATA = vito.xml vcontrold.xml
