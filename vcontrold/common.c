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
/* #include"parser.h" */




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
	
void logIT (int class,char *string) {

	
	time_t t;
	char *tPtr;
	char *cPtr;
	time(&t);
	tPtr=ctime(&t);
	char dbg[1000];
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
		sprintf(dbg,"DEBUG:%s: %s\n",tPtr,string);
		write(dbgFD,dbg,strlen(dbg));
	}

	if (!debug && (class  > LOG_NOTICE)) 
		 return; 

	pid=getpid();

	if (syslogger)
		syslog(class,"%s",string);
	if (logFD) {
		fprintf(logFD,"[%d] %s: %s\n",pid,tPtr,string);
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
	/* wandelt 1-2stellige Hex-Strings in Character */
	size_t n;
	n=strlen(hex);
	int i;
	int result=0;
	int val;
	char string[1000];
	char c[2];
	for(i=0;i<n;i++) {
		if (!isxdigit(hex[i])) {
			sprintf(string,"Kein Hex Zeichen %x",hex[i]);
			logIT(LOG_WARNING,string);
			continue;
		}
		switch(hex[i]) {
			case '0': case '1': case '2': case '3': case '4': 
			case '5': case '6': case '7': case '8': case '9': 
				c[0]=hex[i];
				c[1]='\0';
				val=atoi(c);
				break;
			case 'a': case 'A':
				val=10;
				break;
			case 'b': case 'B':
				val=11;
				break;
			case 'c': case 'C':
				val=12;
				break;
			case 'd': case 'D':
				val=13;
				break;
			case 'e': case 'E':
				val=14;
				break;
			case 'f': case 'F':
				val=15;
				break;
			default:
				sprintf(string,"Fehler Typumwandlung Hex->Chr %s",hex);
				logIT(LOG_ERR,string);
				return(-1);
				break;
		}
		int factor=1;
		int f;
		for(f=0;f<(n-i-1);f++)
			factor*=16;
		result+=factor*val;
	}
	return((char) result);		
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


































