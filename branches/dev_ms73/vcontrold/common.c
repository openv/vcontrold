/*
 * Testprogramm fuer Vito- Abfrage
 *  */
/* $Id: common.c 26 2008-03-20 20:56:09Z marcust $ */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include"common.h" 


/* globale Variablen */
int syslogger=0;
int debug=0;
FILE *logFD;
char errMsg[2000];
int errClass=99;
int dbgFD=-1;

int initLog(int useSyslog, char *logfile,int debugSwitch) {
	/* oeffnet bei Bedarf syslog oder log-Datei */
	if (useSyslog) {
		syslogger=1;
		openlog("vito",LOG_PID,LOG_LOCAL0);
		syslog(LOG_LOCAL0,"vito gestartet");
	}
	if (*logfile) {
		logFD=fopen(logfile,"a");
		if (!logFD) {
			printf("Konnte %s nicht oeffnen %s",logfile, strerror (errno));
			return(0) ;
		}
	}
	debug=debugSwitch;
	bzero(errMsg,sizeof(errMsg));
	return(1);
 
}

const char* LevelName[LOG_DEBUG+1] = { "EME",
                                       "ALE",
                                       "CRI",
                                       "ERR",
                                       "WAR",
                                       "NOT",
                                       "INF",
                                       "DEB" };

void logWrite(int class,char *string, char* sourcefile, long sourceline)
{
	time_t t;
	char *tPtr;
	char *cPtr;
	time(&t);
	tPtr=ctime(&t);
	char dbg[LOG_BUFFER_SIZE];
	int pid;

	if (class <= LOG_ERR)  {
		strncat(errMsg,string,sizeof(errMsg)-strlen(string)-2);
		strcat(errMsg,"\n");
	}

	errClass=class;
	/* Steuerzeichen verdampfen */
	cPtr=tPtr;
	while (*cPtr) {
		if (iscntrl(*cPtr))
			*cPtr=' ';
		cPtr++;
	}

	if (dbgFD>=0) {
		/* der Debug FD ist gesetzt und wir senden die Infos zuerst dort hin */
		bzero(dbg,sizeof(dbg));
		snprintf(dbg, LOG_BUFFER_SIZE-2, "DEBUG:%s: %s (%s:%ld)\n", tPtr, string, sourcefile, sourceline);
		write(dbgFD,dbg,strlen(dbg));
	}

	if (!debug && (class  > LOG_NOTICE)) 
		 return; 

	pid=getpid();

	if (syslogger)
		syslog(class,"%s",string);
	if (logFD) {
		fprintf(logFD,"[%d] %s: %s (%s:%ld)\n", pid, tPtr, string, sourcefile, sourceline);
		fflush(logFD);
	}
	/* Ausgabe nur, wenn 2 als STDERR geoeffnet ist */
	if(isatty(2))
		fprintf(stderr,"[%d] %s: %s\n",pid,tPtr,string);
}

void sendErrMsg(int fd) {
	char string[1010];

	if ((fd>=0) && (errClass<=3)) {
		sprintf(string,"ERR: %s",errMsg);
		write(fd,string,strlen(string));
		errClass=99; /* damit wird sie nur ein mal angezeigt */
		bzero(errMsg,sizeof(errMsg));
	}
}

void setDebugFD(int fd) {
	dbgFD=fd;
}

		
char hex2chr(char *hex) {
	char buffer[16];
	int hex_value=-1;
	
	sprintf(buffer, "0x%s", hex);
	if (sscanf(hex, "%x", &hex_value) != 1) {
		char string[64];
		VCLog(LOG_WARNING, "Ungültige Hex Zeichen in %s", hex);
	}
	return hex_value;
}

int char2hex(char *outString, const char *charPtr, int len) {
	int n;
	char string[MAXBUF];
	bzero(string,sizeof(string));
	for (n=0;n<len;n++) {
		unsigned char byte=*charPtr++ & 255;
		sprintf(string,"%02X ",byte);
		strcat(outString,string);
	}
	/* letztes Leerzeichen verdampfen */
	outString[strlen(outString)-1]='\0';
	return(len);
}

short string2chr(char *line,char *buf,short bufsize) {
	char *sptr;
	short count;
	
	count=0;

	sptr=strtok(line," ");
	do {
		if (*sptr==' ')
			continue;
		buf[count++]=hex2chr(sptr);
	} while((sptr=strtok(NULL," ")) && (count<bufsize));

	return(count);
}
