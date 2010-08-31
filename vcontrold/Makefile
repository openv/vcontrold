# Makefile fuer vitocontrol Daemon
# $Id: Makefile 35 2008-05-05 17:28:07Z marcust $

CC = gcc
LDFLAGS = -lxml2 
CFLAGS = -g -I/usr/local/include/libxml2/ -L/usr/local/lib/
MAKE = make
SHELL = /bin/sh
MAKEFLAGS = b

SRCS = vcontrold.c parser.c io.c common.c socket.c xmlconfig.c arithmetic.c client.c unit.c
OBJS = vcontrold.o parser.o io.o common.o socket.o xmlconfig.o arithmetic.o unit.o
HDRS =                       parser.h io.h common.h socket.h xmlconfig.h arithmetic.h unit.h
BIN = vcontrold
VERSION=0.9
PACKAGE = vcontrold-$(VERSION).tgz
FILES = $(SRCS) $(HDRS) Makefile Makefile.arm Makefile.mips $(BIN).xml

CL_OBJS = vclient.o common.o socket.o io.o client.o 
CL_BIN = vclient

.h.c.o:
	$(CC) $(CFLAGS) -c $<


$(BIN): $(OBJS) 
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@ 

$(CL_BIN): $(CL_OBJS) 

	$(CC) $(CFLAGS) $(LDFLAGS) $(CL_OBJS) -o $@ 



#$(OBJS): $(SRCS)
#	$(CC) $(CFLAGS) $(LDFLAGS) -c $?

all: $(BIN)
clean:
	 rm -rf $(OBJS) $(BIN) $(CL_OBJS) $(CL_BIN)
tar: 
	tar cvzf $(PACKAGE) $(FILES) 
