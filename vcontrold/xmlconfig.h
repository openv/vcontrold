/* xmlconfig.h */
/* $Id: xmlconfig.h 34 2008-04-06 19:39:29Z marcust $ */

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
macroPtr getMacroNode(macroPtr ptr,const char *name);
unitPtr getUnitNode(unitPtr ptr,const char *name);
commandPtr getCommandNode(commandPtr ptr,const char *name);
allowPtr getAllowNode(allowPtr ptr,in_addr_t testIP);
enumPtr getEnumNode(enumPtr prt,char *search,int len);


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
	unsigned char retry;
	unsigned short recvTimeout;
	char bit;
	char nodeType; /* 0==alles kopiert 1==alles Orig. 2== nur Adresse, unit len orig. */
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
	
	
	
