/*  Copyright 2007-2017 the original vcontrold development team

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
*/

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
allowPtr getAllowNode(allowPtr ptr, in_addr_t testIP);
enumPtr getEnumNode(enumPtr prt, char *search, int len);
icmdPtr getIcmdNode(icmdPtr ptr, const char *name);

struct allow {
    char *text;
    in_addr_t ip;
    in_addr_t mask;
    allowPtr next;
} Allow;

struct compile {
    int token;
    char *send;
    int len;
    unitPtr uPtr;
    char *errStr;
    compilePtr next;
} Compile;

struct config {
    char *tty;
    int port;
    char *logfile;
    char *devID;
    devicePtr devPtr;
    allowPtr aPtr;
    int syslog;
    int debug;
} Config;

struct protocol {
    char *name;
    char id;
    macroPtr mPtr;
    icmdPtr icPtr;
    protocolPtr next;
} Protocol;

struct device {
    char *name;
    char *id;
    commandPtr cmdPtr;
    protocolPtr protoPtr;
    devicePtr next;
} Device;

struct unit {
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

struct macro {
    char *name;
    char *command;
    macroPtr next;
} Macro;

struct command {
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
    char nodeType; // 0==alles kopiert 1==alles Orig. 2== nur Adresse, unit len orig.
    compilePtr cmpPtr;
    char *description;
    commandPtr next;
} Command;

struct icmd {
    char *name;
    char *send;
    unsigned char retry;
    unsigned short recvTimeout;
    icmdPtr next;
} iCmd;

struct enumerate {
    char *bytes;
    int len;
    char *text;
    enumPtr next;
} Enumerate;

#endif // XMLCONFIG_H
