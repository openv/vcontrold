/*
 * Vito-Control Daemon fuer Vito- Abfrage
 *  */
/* $Id: vcontrold.c 34 2008-04-06 19:39:29Z marcust $ */
#include <stdlib.h>
#include <stdio.h> 
#include <ctype.h>
#include <errno.h>
#include <signal.h>  
#include <syslog.h> 
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <time.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>


#include"io.h"
#include "common.h"
#include "xmlconfig.h"
#include "parser.h"
#include "socket.h"
#include "prompt.h"
#include "semaphore.h"
#include "framer.h"

#ifdef __CYGWIN__
#define XMLFILE "vcontrold.xml"
#define INIOUTFILE "sim-%s.ini"
#else
#define XMLFILE "/etc/vcontrold/vcontrold.xml"
#define INIOUTFILE "/tmp/sim-%s.ini"
#endif

#define VERSION_DAEMON "0.98.2_IPv6"


/* Globale Variablen */
char *xmlfile = XMLFILE;
FILE *iniFD=NULL;
int makeDaemon=1;
int inetversion=0;

/* in xmlconfig.c definiert */
extern protocolPtr protoPtr; 
extern unitPtr uPtr;
extern devicePtr devPtr;
extern configPtr cfgPtr; 



/* Deklarationen */

int readCmdFile(char *filename,char *result,int *resultLen,char *device );
int interactive(int socketfd,char *device);
void printHelp(int socketfd); 
int rawModus (int socketfd,char *device);
static void sigPipeHandler(int signo);
static void sigHupHandler(int signo);
short checkIP(char *ip); 
int reloadConfig();




void usage() {
	printf("usage: vcontrold [-x|--xmlfile xml-file] [-d|--device <device>] [-l|--logfile <logfile>] [-p|--port port] [-s|--syslog] [-n|--nodaemon] [-i|--vsim] [-g|--debug] [-4|--inet4] [-6|--inet6]\n");
	exit(1);
}

short checkIP(char *ip) {
	allowPtr aPtr;
	if ((aPtr = getAllowNode(cfgPtr->aPtr,inet_addr(ip)))) {
		logIT(LOG_INFO,"%s in allowList (%s)",ip,aPtr->text);
		return(1);
	}
	else { 
		logIT(LOG_INFO,"%s nicht in allowList",ip);
		return(0);
	}
}

int reloadConfig() {
	if (parseXMLFile(xmlfile)) {
		compileCommand(devPtr,uPtr);
		logIT(LOG_NOTICE,"XMLFile %s neu geladen",xmlfile);
		return(1);
	}
	else {
		logIT(LOG_ERR,"Laden von XMLFile %s gescheitert",xmlfile);
		return(0);
	}
}
	
		 
int readCmdFile(char *filename,char *result,int *resultLen,char *device ) {
	FILE *cmdPtr;
	char line[MAXBUF];
	char string[256];
	char recvBuf[MAXBUF];
	char *resultPtr=result;
	int fd;
	int count=0;
	//void *uPtr;
	//int maxResLen=*resultLen;
	*resultLen=0; /* noch keine Zeichen empfangen :-) */

	/* das Device wird erst geoeffnet, wenn wir was zu tun haben */
	vcontrol_semget(); // todo semjfi
	if ((fd=framer_openDevice(device, cfgPtr->devPtr->protoPtr->id))== -1) {
		vcontrol_semrelease(); // todo semjfi
		logIT(LOG_ERR,"Fehler beim oeffnen %s",device);
		result="\0";
		*resultLen=0;
		return(0);
	}
	
	cmdPtr=fopen(filename,"r");
	if (!cmdPtr) {
		logIT(LOG_ERR,"Kann Cmd File %s nicht oeffnen",filename);
		result="\0";
		*resultLen=0;
		framer_closeDevice(fd);
		vcontrol_semrelease(); // todo semjfi
		return(0);
	} 
	logIT(LOG_INFO,"Lese Cmd File %s",filename);
	/* Queue leeren */
	// todo semjfi vcontrol_semget();
        tcflush(fd,TCIOFLUSH);
        // todo semjfi vcontrol_semrelease();
	while(fgets(line,MAXBUF-1,cmdPtr)) {
		/* \n verdampfen */
		line[strlen(line)-1]='\0';
		bzero(recvBuf,sizeof(recvBuf));
		// todo semjfi vcontrol_semget();
		count=execCmd(line,fd,recvBuf,sizeof(recvBuf));
		// todo semjfi vcontrol_semrelease();
		int n;
		char *ptr;
		ptr=recvBuf;
		char buffer[MAXBUF];
		bzero(buffer,sizeof(buffer));
		for(n=0;n<count;n++) { /* wir haben Zeichen empfangen */
			*resultPtr++=*ptr;
			(*resultLen)++;
			bzero(string,sizeof(string));
			unsigned char byte=*ptr++ & 255;
			snprintf(string, sizeof(string),"%02X ",byte);
			strcat(buffer,string);
			if (n >= MAXBUF-3)
				break;
		}
		if (count -1) {
			/* timeout */
		}
		if (count) {
			logIT(LOG_INFO,"Empfangen: %s",buffer);
		}
			
	}
	framer_closeDevice(fd);
	vcontrol_semrelease(); // todo semjfi
	fclose(cmdPtr);
	return(1);
}
void printHelp(int socketfd) {
	char string[]=" \
close: schliesst Verbindung zum Device\n \
commands: Listet alle in der XML definierten Kommandos fuer das Protokoll\n \
debug on|off: Debug Informationen an/aus\n \
detail <command>: Zeigt detaillierte Informationen zum <command> an\n \
device: In der XML gewaehltes Device\n \
protocol: Aktives Protokoll\n \
raw: Raw Modus, Befehle WAIT,SEND,RECV,PAUSE Abschluss mit END\n \
reload: Neu-Laden der XML Konfiguration\n \
unit on|off: Wandlung des Ergebnisses lsut definierter Unit an/aus\n \
version: Zeigt die Versionsnummer an\n \
quit: beendet die Verbindung\n";
	Writen(socketfd,string,strlen(string));
}

int rawModus(int socketfd,char *device) {
	/* hier schreiben wir alle ankommenden Befehle in eine temp. Datei */
	/* diese Datei wird dann an readCmdFile uerbegeben */
	char readBuf[MAXBUF];
	char string[256];
	
	#ifdef __CYGWIN__
	char tmpfile[]="vitotmp-XXXXXX";
	#else
	char tmpfile[]="/tmp/vitotmp-XXXXXX";
	#endif
	
	FILE *filePtr;
	char result[MAXBUF];
	int resultLen;

	if (!mkstemp(tmpfile)) {
		/* noch ein Versuch */
		if (!mkstemp(tmpfile)) {
			logIT1(LOG_ERR,"Fehler Erzeugung mkstemp");
			return(0);
		}
	}
	filePtr=fopen(tmpfile,"w+");
	if (!filePtr) {
		logIT(LOG_ERR, "Kann Tmp File %s nicht anlegen",tmpfile);
		return(0);
	} 
	logIT(LOG_INFO,"Raw Modus: Temp Datei: %s",tmpfile);
	while(Readline(socketfd,readBuf,sizeof(readBuf))) {
		/* hier werden die einzelnen Befehle geparst */
		if (strstr(readBuf,"END")==readBuf) {
			fclose(filePtr);
			resultLen=sizeof(result);
			readCmdFile(tmpfile,result,&resultLen,device);
			if (resultLen) { /* es wurden Zeichen empfangen */
				char buffer[MAXBUF];
				bzero(buffer,sizeof(buffer));
				char2hex(buffer,result,resultLen);
				snprintf(string, sizeof(string),"Result: %s\n",buffer);
				Writen(socketfd,string,strlen(string));
			}
			remove(tmpfile);	
			return(1);
		}
		logIT(LOG_INFO,"Raw: Gelesen: %s",readBuf);
		/*
		int n;
		if ((n=fputs(readBuf,filePtr))== 0) {
		*/
		if (fputs(readBuf,filePtr)== EOF) {
			logIT1(LOG_ERR,"Fehler beim schreiben tmp Datei");
		}
		else {
			/* debug stuff */
		}
			
	}	
	return 0;			// is this correct?
}

int interactive(int socketfd,char *device ) {
	
	char readBuf[1000];
	char *readPtr;
	char prompt[]=PROMPT;
	char bye[]=BYE;
	char string[256];
	commandPtr cPtr;
	commandPtr pcPtr;
	int fd=-1;
	short count=0;
	short rcount=0;
	short noUnit=0;
	char recvBuf[MAXBUF];
	char pRecvBuf[MAXBUF];
	char sendBuf[MAXBUF];
	char cmd[MAXBUF];
	char para[MAXBUF];
	char *ptr;
	short sendLen;
	char buffer[MAXBUF];

	Writen(socketfd,prompt,strlen(prompt));
	bzero(readBuf,sizeof(readBuf));
	
	
	while((rcount=Readline(socketfd,readBuf,sizeof(readBuf)))) {
				
		sendErrMsg(socketfd);
		/* Steuerzeichen verdampfen */
		/*readPtr=readBuf+strlen(readBuf); **/
		readPtr=readBuf+rcount;
		while(iscntrl(*readPtr))
			*readPtr--='\0';
		logIT(LOG_INFO,"Befehl: %s",readBuf);

		/* wir trennen Kommando und evtl. Optionen am ersten Blank */
		bzero(cmd,sizeof(cmd));
		bzero(para,sizeof(para));
		if((ptr=strchr(readBuf,' '))) {
			strncpy(cmd,readBuf,ptr-readBuf);
			strcpy(para,ptr+1);
		}
		else 
			strcpy(cmd,readBuf);

		/* hier werden die einzelnen Befehle geparst */
		if (strstr(readBuf,"help")==readBuf) {
			printHelp(socketfd);
		}
		else if(strstr(readBuf,"quit")==readBuf) {
			Writen(socketfd,bye,strlen(bye));
			if (fd != -1) {
			  framer_closeDevice(fd);
			  vcontrol_semrelease(); // todo semjfi
			}
			return(1);
		}
		else if(strstr(readBuf,"debug on")==readBuf) {
			setDebugFD(socketfd);
		}
		else if(strstr(readBuf,"debug off")==readBuf) {
			setDebugFD(-1);
		}
		else if(strstr(readBuf,"unit off")==readBuf) {
			noUnit=1;
		}
		else if(strstr(readBuf,"unit on")==readBuf) {
			noUnit=0;
		}
		else if(strstr(readBuf,"reload")==readBuf) {
			if (reloadConfig()) {
				bzero(string,sizeof(string));
				snprintf(string, sizeof(string),"XMLFile %s neu geladen\n",xmlfile);
				Writen(socketfd,string,strlen(string));
				/* falls wir einen Vater haben (Daemon Mode) bekommt er ein sighup */
				if (makeDaemon) {
					kill(getppid(),SIGHUP);
				} 
			}
			else {
				bzero(string,sizeof(string));
				snprintf(string, sizeof(string),"Laden von XMLFile %s gescheitert, nutze alte Konfig\n",xmlfile);
				Writen(socketfd,string,strlen(string));
			}

		}
		else if(strstr(readBuf,"raw")==readBuf) {
			rawModus(socketfd,device);
		}
		else if(strstr(readBuf,"close")==readBuf) {
			framer_closeDevice(fd);
			vcontrol_semrelease(); // todo semjfi
			snprintf(string, sizeof(string),"%s geschlossen\n",device);
			Writen(socketfd,string,strlen(string));
			fd=-1;
		}
/* 		else if(strstr(readBuf,"open")==readBuf) {
			if ((fd<0) && (fd=openDevice(device))== -1) {
				snprintf(string, sizeof(string),"Fehler beim oeffnen %s",device);
				logIT(LOG_ERR,string);
			}
			snprintf(string, sizeof(string),"%s geoeffnet\n",device);
			Writen(socketfd,string,strlen(string));
		}
*/
		else if(strstr(readBuf,"commands")==readBuf) {
			cPtr=cfgPtr->devPtr->cmdPtr; 
			while (cPtr) {
				if (cPtr->addr) {
					bzero(string,sizeof(string));
					snprintf(string, sizeof(string),"%s: %s\n",cPtr->name,cPtr->description);
					Writen(socketfd,string,strlen(string));
				}
				cPtr=cPtr->next;
			}

		}
		else if(strstr(readBuf,"protocol")==readBuf) {
			bzero(string,sizeof(string));
			snprintf(string, sizeof(string),"%s\n",cfgPtr->devPtr->protoPtr->name);
			Writen(socketfd,string,strlen(string));
		}
		else if(strstr(readBuf,"device")==readBuf) {
			bzero(string,sizeof(string));
			snprintf(string, sizeof(string),"%s (ID=%s) (Protocol=%s)\n",cfgPtr->devPtr->name,
							cfgPtr->devPtr->id,
							cfgPtr->devPtr->protoPtr->name);
			Writen(socketfd,string,strlen(string));
		}
		else if(strstr(readBuf,"version")==readBuf) {
			bzero(string,sizeof(string));
			snprintf(string, sizeof(string),"Version: %s\n",VERSION_DAEMON);
			Writen(socketfd,string,strlen(string));
		}
		/* Ist das Kommando in der XML definiert ? */
		//else if(readBuf && (cPtr=getCommandNode(cfgPtr->devPtr->cmdPtr,cmd))&& (cPtr->addr)) {
		else if((cPtr=getCommandNode(cfgPtr->devPtr->cmdPtr,cmd))&& (cPtr->addr)) { 
			bzero(string,sizeof(string));
			bzero(recvBuf,sizeof(recvBuf));
			bzero(sendBuf,sizeof(sendBuf));
			bzero(pRecvBuf,sizeof(pRecvBuf));


			/* Falls Unit Off wandeln wir die uebergebenen Parameter in Hex */
			/* oder keine Unit definiert ist */
			bzero(sendBuf,sizeof(sendBuf));
			if((noUnit|!cPtr->unit) && *para) {
				if((sendLen=string2chr(para,sendBuf,sizeof(sendBuf)))==-1) {
					logIT(LOG_ERR,"Kein Hex string: %s",para);
					sendErrMsg(socketfd);
					if (!Writen(socketfd,prompt,strlen(prompt))) {
						sendErrMsg(socketfd);
						framer_closeDevice(fd);
						vcontrol_semrelease(); // todo semjfi
						return(0);
					}
					continue;
				}
				/* falls sendLen > als len der Befehls, nehmen wir len */
				if (sendLen > cPtr->len) {
					logIT(LOG_WARNING,"Laenge des Hex Strings > Sendelaenge des Befehls, sende nur %d Byte",cPtr->len);
					sendLen=cPtr->len;
				}
			}
			else if (*para) { /* wir kopieren die Parameter, darum kuemert sich execbyteCode selbst */
				strcpy(sendBuf,para);
				sendLen=strlen(sendBuf);
			}
			if (iniFD) 
				fprintf(iniFD,";%s\n",readBuf); 
				

			/* das Device wird erst geoeffnet, wenn wir was zu tun haben */
			/* aber nur, falls es nicht schon offen ist */

			if (fd < 0) {
				/* As one vclient call opens the link once, all is seen a transaction
				 * This may cause trouble for telnet sessions, as the whole session is
				 * one link activity, even more commands are given within.
				 * This is related to a accept/close on a server socket
				 */
				vcontrol_semget(); // everything on link is a transaction - all commands //todo semjfi

				if ((fd = framer_openDevice(device,	cfgPtr->devPtr->protoPtr->id)) == -1) {
					logIT(LOG_ERR, "Fehler beim oeffnen %s", device);
					sendErrMsg(socketfd);
					if (!Writen(socketfd, prompt, strlen(prompt))) {
						sendErrMsg(socketfd);
						framer_closeDevice(fd);
						vcontrol_semrelease();  //todo semjfi
						return (0);
					}
					framer_closeDevice(fd);
					vcontrol_semrelease();
					continue;
				}
			}
#if 1 == 2 // todo semjfi
			if ((fd<0) && (fd=framer_openDevice(device, cfgPtr->devPtr->protoPtr->id))== -1) {
				logIT(LOG_ERR,"Fehler beim oeffnen %s",device);
				sendErrMsg(socketfd);
				if (!Writen(socketfd,prompt,strlen(prompt))) {
					sendErrMsg(socketfd);
					framer_closeDevice(fd);
					return(0);
				}
				continue;
			}

			vcontrol_semget();
#endif

			/* falls ein Pre-Kommando definiert wurde, fuehren wir dies zuerst aus */
			if (cPtr->precmd &&(pcPtr=getCommandNode(cfgPtr->devPtr->cmdPtr,cPtr->precmd))) {
				logIT(LOG_INFO,"Fuehre Pre Kommando %s aus",cPtr->precmd);

				if (execByteCode(pcPtr->cmpPtr,fd,pRecvBuf,sizeof(pRecvBuf),sendBuf,sendLen,1,pcPtr->bit,pcPtr->retry,pRecvBuf,pcPtr->recvTimeout)==-1) {	
				  // todo semjfi vcontrol_semrelease();
				  framer_closeDevice(fd);
				  vcontrol_semrelease();
				  logIT(LOG_ERR,"Fehler beim ausfuehren von %s",readBuf);
				  sendErrMsg(socketfd);
				  break;
				}
				else {
					bzero(buffer,sizeof(buffer));
					char2hex(buffer,pRecvBuf,pcPtr->len);
					logIT(LOG_INFO,"Ergebnis Pre-Kommand: %s",buffer);
				}
	
					
			}
				
			/* wir fuehren den Bytecode aus, 
			       -1 -> Fehler  
				0 -> Formaterierter String
				n -> Bytes in Rohform */

			count=execByteCode(cPtr->cmpPtr,fd,recvBuf,sizeof(recvBuf),sendBuf,sendLen,noUnit,cPtr->bit,cPtr->retry,pRecvBuf,cPtr->recvTimeout);
			// todo semjfi vcontrol_semrelease();

			if (count==-1) {
				logIT(LOG_ERR,"Fehler beim ausfuehren von %s",readBuf);
				sendErrMsg(socketfd);
			}
			else if (*recvBuf && (count==0)) { /* Unit gewandelt */
				
				logIT1(LOG_INFO,recvBuf);
				snprintf(string, sizeof(string),"%s\n",recvBuf);
				Writen(socketfd,string,strlen(string));
			}
			else {
				int n;
				char *ptr;
				ptr=recvBuf;
				char buffer[MAXBUF];
				bzero(buffer,sizeof(buffer));
				for(n=0;n<count;n++) { /* wir haben Zeichen empfangen */
					bzero(string,sizeof(string));
					unsigned char byte=*ptr++ & 255;
					snprintf(string, sizeof(string),"%02X ",byte);
					strcat(buffer,string);
					if (n >= MAXBUF-3)
						break;
				}
				if (count) {
					snprintf(string, sizeof(string),"%s\n",buffer);
					Writen(socketfd,string,strlen(string));
					logIT(LOG_INFO,"Empfangen: %s",buffer);
				}
			}
			if (iniFD)
				fflush(iniFD);
		}
		else if(strstr(readBuf,"detail")==readBuf) {
			readPtr=readBuf+strlen("detail");
			while(isspace(*readPtr))
				readPtr++;
			/* Ist das Kommando in der XML definiert ? */
		        if(readPtr && (cPtr=getCommandNode(cfgPtr->devPtr->cmdPtr,readPtr))) { 
				bzero(string,sizeof(string));
				snprintf(string, sizeof(string),"%s: %s\n",cPtr->name,cPtr->send);
				Writen(socketfd,string,strlen(string));
				/* Error String definiert */
				char buf[MAXBUF];
				bzero(buf,sizeof(buf));
				if (cPtr->errStr && char2hex(buf,cPtr->errStr,cPtr->len)) {
					snprintf(string, sizeof(string),"\tError bei (Hex): %s",buf);
					Writen(socketfd,string,strlen(string));
				}
				/* recvTimeout ?*/
				if (cPtr->recvTimeout){
					snprintf(string, sizeof(string),"\tRECV Timeout: %d ms\n",cPtr->recvTimeout);
					Writen(socketfd,string,strlen(string));
				}
				/* Retry definiert ? */
				if (cPtr->retry) {
					snprintf(string, sizeof(string),"\tRetry: %d\n",cPtr->retry);
					Writen(socketfd,string,strlen(string));
				}
				/* Ist Bit definiert ?*/
				if (cPtr->bit > 0) {
					snprintf(string, sizeof(string),"\tBit (BP): %d\n",cPtr->bit);
					Writen(socketfd,string,strlen(string));
				}
				/* Pre-Command definiert ?*/
				if (cPtr->precmd){
					snprintf(string, sizeof(string),"\tPre-Kommando (P0-P9): %s\n",cPtr->precmd);
					Writen(socketfd,string,strlen(string));
				}
					
				/* Falls eine Unit verwendet wurde, geben wir das auch noch aus */
				compilePtr cmpPtr;
				cmpPtr=cPtr->cmpPtr;
				while(cmpPtr) {
					if (cmpPtr && cmpPtr->uPtr) { /* Unit gefunden */
						char *gcalc;
						char *scalc;
						/* wir unterscheiden die Rechnerei nach get und setaddr */
						if(cmpPtr->uPtr->gCalc && *cmpPtr->uPtr->gCalc)
							gcalc=cmpPtr->uPtr->gCalc;
						else 
							gcalc=cmpPtr->uPtr->gICalc;
						if(cmpPtr->uPtr->sCalc && *cmpPtr->uPtr->sCalc)
							scalc=cmpPtr->uPtr->sCalc;
						else 
							scalc=cmpPtr->uPtr->sICalc;

						snprintf(string, sizeof(string),"\tUnit: %s (%s)\n\t  Type: %s\n\t  Get-Calc: %s\n\t  Set-Calc: %s\n\t Einheit: %s\n",
								cmpPtr->uPtr->name,cmpPtr->uPtr->abbrev,
								cmpPtr->uPtr->type,
								gcalc,
								scalc,
								cmpPtr->uPtr->entity);
						Writen(socketfd,string,strlen(string));
						/* falls es sich um ein enum handelt, gibts noch mehr */
						/* oder sonstwo enums definiert sind */
						if(cmpPtr->uPtr->ePtr) {
							enumPtr ePtr;
							ePtr=cmpPtr->uPtr->ePtr;
							char dummy[20]; 
							while(ePtr) {
								bzero(dummy,sizeof(dummy));
								if (!ePtr->bytes) 
									strcpy(dummy,"<default>");
								else 
									char2hex(dummy,ePtr->bytes,ePtr->len);
								snprintf(string, sizeof(string),"\t  Enum Bytes:%s Text:%s\n",dummy,ePtr->text);
								Writen(socketfd,string,strlen(string));
								ePtr=ePtr->next;
							}
						} 
					}
					cmpPtr=cmpPtr->next;
				}
			}
			else {
				bzero(string,sizeof(string));
				snprintf(string, sizeof(string),"ERR: command %s unbekannt\n",readPtr);
				Writen(socketfd,string,strlen(string));
			}
		} 
		else if(*readBuf) { 
			if (!Writen(socketfd,UNKNOWN,strlen(UNKNOWN))) { 
				sendErrMsg(socketfd);
				framer_closeDevice(fd);
				vcontrol_semrelease(); // todo semjfi
				return(0);
			}
		
		}
		bzero(string,sizeof(string));
		sendErrMsg(socketfd);
		snprintf(string, sizeof(string),"%s",prompt); //,readBuf);  // is this needed? what does it do?
		if (!Writen(socketfd,prompt,strlen(prompt))) {
			sendErrMsg(socketfd);
			framer_closeDevice(fd);
			vcontrol_semrelease(); // todo semjfi
			return(0);
		}
		bzero(readBuf,sizeof(readBuf));
	}
	sendErrMsg(socketfd);
	framer_closeDevice(fd);
	vcontrol_semrelease(); // todo semjfi
	return(0);
} 

static void sigPipeHandler(int signo) {
	logIT1(LOG_ERR,"SIGPIPE empfangen");
}

static void sigHupHandler(int signo) {
	logIT1(LOG_NOTICE,"SIGHUP empfangen");
	reloadConfig();
}

static void sigTermHandler (int signo) {
	logIT1(LOG_NOTICE, "SIGTERM empfangen");
	vcontrol_semfree();
	exit(1);
}

/* hier gehts los */

int main(int argc, char* argv[]) {

	/* Auswertung der Kommandozeilenschalter */
	char *device = NULL;
	char *cmdfile = NULL;
	char *logfile = NULL;
	static int useSyslog=0;
	static int debug=0;
	static int verbose = 0;
	int tcpport=0;
	static int simuOut=0;
	int opt;

	while (1)
	{

		static struct option long_options[] =
		{
			{"commandfile",	required_argument,	0, 'c'},
			{"device",		required_argument,	0, 'd'},
			{"debug",		no_argument,		&debug, 1},
			{"vsim",		no_argument,		&simuOut, 1},
			{"logfile",		required_argument,	0, 'l'},
			{"nodaemon",	no_argument,		&makeDaemon, 0},
			{"port",		required_argument,	0, 'p'},
			{"syslog",		no_argument,		&useSyslog, 1},
			{"xmlfile",		required_argument,	0, 'x'},
			{"verbose",		no_argument,		&verbose, 1},
			{"inet4",		no_argument,		&inetversion, 4},
			{"inet6",		no_argument,		&inetversion, 6},
			{"help",		no_argument,		0,	0},
			{0,0,0,0}
		};
		/* getopt_long stores the option index here. */
		int option_index = 0;
		opt = getopt_long (argc, argv, "c:d:gil:np:svx:46",
						   long_options, &option_index);
		
		/* Detect the end of the options. */
		if (opt == -1)
			break;
		
		switch (opt) {
			case 0:
				/* If this option sets a flag, we do nothing for now */
				if (long_options[option_index].flag != 0)
					break;
				if (verbose) {
					printf("option %s", long_options[option_index].name);
					if (optarg)
						printf(" with arg %s", optarg);
					printf("\n");
				}
				if (strcmp("help", long_options[option_index].name) == 0) {
					usage();
				}
				break;

			case '4':
				inetversion = 4;
				break;
			case '6':
				inetversion = 6;
				break;

			case 'c':
				cmdfile = optarg;
				break;

			case 'd':
				device = optarg;
				break;

			case 'g':
				debug = 1;
				break;

			case 'i':
				simuOut = 1;
				break;

			case 'l':
				logfile = optarg;
				break;

			case 'n':
				makeDaemon = 0;
				break;

			case 'p':
				tcpport = atoi(optarg);
				break;

			case 's':
				useSyslog = 1;
				break;

			case 'v':
				puts ("option -v\n");
				verbose = 1;
				break;

			case 'x':
				xmlfile = optarg;
				break;

			case '?':
				/* getopt_long already printed an error message. */
				usage();
				break;
 				
			default:
				abort();
		}
	}

	initLog(useSyslog,logfile,debug);

	if (!parseXMLFile(xmlfile)){
		fprintf(stderr,"Fehler beim Laden von %s, terminiere!\n",xmlfile); 
		exit(1);
	}


	/* es wurden die beiden globalen Variablen cfgPtr und protoPtr gefuellt */
	if (cfgPtr) { 
		if (!tcpport)
			tcpport=cfgPtr->port;
		if (!device)
			device = cfgPtr->tty;
		if (!logfile)
			logfile = cfgPtr->logfile;
		if (!useSyslog)
			useSyslog=cfgPtr->syslog;
		if (!debug)
			debug=cfgPtr->debug;
	}

	if (!initLog(useSyslog,logfile,debug)) 
		exit(1);

	if (signal(SIGHUP,sigHupHandler)== SIG_ERR) {
		logIT1(LOG_ERR,"Fehler beim Signalhandling SIGHUP");
		exit(1);
	}
		
	if (signal(SIGQUIT, sigTermHandler) == SIG_ERR) {
		logIT(LOG_ERR, "Fehler beim Signalhandling SIGTERM: %s", strerror(errno));
		exit(1);
	}
	/* falls -i angegeben wurde, loggen wir die Befehle im Simulator INI Format */
	if (simuOut) {
		char file[100];
		snprintf(file,sizeof(file), INIOUTFILE,cfgPtr->devID);
		if (!(iniFD=fopen(file,"w"))) {
			logIT(LOG_ERR,"Konnte Simulator INI File %s nicht anlegen",file);
		}
		fprintf(iniFD,"[DATA]\n");
	}	

	/* die Macros werden ersetzt und die zu sendenden Strings in Bytecode gewandelt */
	compileCommand(devPtr,uPtr);

	int fd = 0;
	char result[MAXBUF];
	int resultLen=sizeof(result);
	int sid;

	if (tcpport) {
		if (makeDaemon) { 
			int pid;
			/* etwas Siganl Handling */
			if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
				logIT1(LOG_ERR,"Fehler beim Signalhandling SIGCHLD");
			}
			
			pid=fork();
			if (pid <0) {
				logIT(LOG_ERR,"fork fehlgeschlagen (%d)",pid);
				exit(1);
			}
			if (pid > 0) 
				exit(0); /* Vater wird beendet, Kind laueft weiter */

			/* ab hier laueft nur noch das Kind */

			/* FD schliessen */
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			umask(0);

			sid=setsid();
			if(sid <0) {
				logIT1(LOG_ERR, "setsid fehlgeschlagen");
				exit(1);
			}
			if (chdir("/") <0) {
				logIT1(LOG_ERR, "chdir / fehlgeschlagen");
				exit(1);
			}	

		}

		vcontrol_seminit();
			
		int sockfd=-1;
		int listenfd=-1;
		/* Zeiger auf die Funktion checkIP */

		short (*checkP)(char *);

		if (cfgPtr->aPtr)  /* wir haben eine allow Liste */
			checkP=checkIP;
		else
			checkP=NULL;	

		listenfd=openSocket(tcpport);
		while(1) {
			sockfd=listenToSocket(listenfd,makeDaemon,checkP);
			if (signal(SIGPIPE,sigPipeHandler)== SIG_ERR) {
				logIT1(LOG_ERR,"Signal error");
				exit(1);
			}
			if (sockfd>=0) {
				/* Socket hat fd zurueckgegeben, den Rest machen wir in interactive */
				interactive(sockfd,device);
				closeSocket(sockfd);
				setDebugFD(-1);
				if (makeDaemon) {
					logIT(LOG_INFO, "Child Prozess pid:%d beendet",getpid());
					exit(0); /* das Kind verabschiedet sich */
				}
			}
			else {
				logIT1(LOG_ERR,"Fehler bei Verbindungsaufbau");
			}
		}
	}
	else
	  vcontrol_seminit();

	if (*cmdfile)
		readCmdFile(cmdfile,result,&resultLen,device);

	vcontrol_semfree();

	close(fd);
	logIT1(LOG_LOCAL0,"vcontrold beendet");
	
	return 0;
}




