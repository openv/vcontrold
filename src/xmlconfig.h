// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef XMLCONFIG_H
#define XMLCONFIG_H
#include <arpa/inet.h>

typedef struct config *configPtr;
typedef struct protocol *protocolPtr;
typedef struct unit *unitPtr;
typedef struct macro *macroPtr;
typedef struct command *commandPtr;
typedef struct compile *compilePtr;
typedef struct device *devicePtr;
typedef struct icmd *icmdPtr;
typedef struct allow *allowPtr;
typedef struct enumerate *enumPtr;

int parseXMLFile(char *filename);
macroPtr getMacroNode(macroPtr ptr, const char *name);
unitPtr getUnitNode(unitPtr ptr, const char *name);
commandPtr getCommandNode(commandPtr ptr, const char *name);
enumPtr getEnumNode(enumPtr prt, char *search, int len);
icmdPtr getIcmdNode(icmdPtr ptr, const char *name);

typedef struct compile {
    int token;
    char *send;
    int len;
    unitPtr uPtr;
    char *errStr;
    compilePtr next;
} Compile;

typedef struct config {
    char *tty;
    int port;
    char *logfile;
    char *pidfile;
    char *username;
    char *groupname;
    char *devID;
    devicePtr devPtr;
    int syslog;
    int debug;
} Config;

typedef struct protocol {
    char *name;
    char id;
    macroPtr mPtr;
    icmdPtr icPtr;
    protocolPtr next;
} Protocol;

typedef struct device {
    char *name;
    char *id;
    commandPtr cmdPtr;
    protocolPtr protoPtr;
    devicePtr next;
} Device;

typedef struct unit {
    char *name;
    char *abbrev;
    char *gCalc;
    char *sCalc;
    char *gICalc;
    char *sICalc;
    char *entity;
    char *type;
    enumPtr ePtr;
    unitPtr next;
} Unit;

typedef struct macro {
    char *name;
    char *command;
    macroPtr next;
} Macro;

typedef struct command {
    char *name;
    char *pcmd;
    char *send;
    char *addr;
    char *unit;
    char *errStr;
    char *precmd;
    unsigned char len;
    int retry;
    unsigned short recvTimeout;
    char bit;
    char nodeType;
    // 0: everything copied
    // 1: everything orig
    // 2: only address, unit len orig
    compilePtr cmpPtr;
    char *description;
    commandPtr next;
} Command;

typedef struct icmd {
    char *name;
    char *send;
    unsigned char retry;
    unsigned short recvTimeout;
    icmdPtr next;
} iCmd;

typedef struct enumerate {
    char *bytes;
    int len;
    char *text;
    enumPtr next;
} Enumerate;

#endif // XMLCONFIG_H
