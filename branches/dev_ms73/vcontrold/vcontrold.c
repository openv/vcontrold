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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>


#include "io.h"
#include "common.h"
#include "xmlconfig.h"
#include "parser.h"
#include "socket.h"
#include "prompt.h"
#include "semaphore.h"

#ifdef __CYGWIN__
#define XMLFILE "vcontrold.xml"
#define INIOUTFILE "sim-%s.ini"
#else
#define XMLFILE "/etc/vcontrold/vcontrold.xml"
#define INIOUTFILE "/tmp/sim-%s.ini"
#endif

#ifndef VERSION
#define VERSION "0.98"
#endif

/* Globale Variablen */
char xmlfile[200]=XMLFILE;
FILE *iniFD=NULL;
int makeDaemon=1;

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
  printf("usage: vcontrold [-x xml-file] [-d <device>] [-l <logfile>] [-p port] [-s] [-n] [-i] [-g]\n");
  exit(1);
}

short checkIP(char *ip) {
	allowPtr aPtr;
	if ((aPtr = getAllowNode(cfgPtr->aPtr,inet_addr(ip)))) {
		VCLog(LOG_INFO,"%s in allowList (%s)", ip, aPtr->text);
		return(1);
	}
	else { 
		VCLog(LOG_INFO, "%s nicht in allowList", ip);
		return(0);
	}
}

int reloadConfig()
{
	if (parseXMLFile(xmlfile)) {
		compileCommand(devPtr,uPtr);
		VCLog(LOG_NOTICE, "XMLFile %s neu geladen", xmlfile);
		return(1);
	}
	else {
		VCLog(LOG_ERR, "Laden von XMLFile %s gescheitert", xmlfile);
		return(0);
	}
}
	
		 



int readCmdFile(char *filename,char *result,int *resultLen,char *device ) {
	FILE *cmdPtr;
	char line[MAXBUF];
	char string[1000];
	char recvBuf[MAXBUF];
	char *resultPtr=result;
	int fd;
	int count=0;
	//void *uPtr;
	//int maxResLen=*resultLen;
	*resultLen=0; /* noch keine Zeichen empfangen :-) */

	/* das Device wird erst geoeffnet, wenn wir was zu tun haben */
	if ((fd=openDevice(device))== -1) {
		VCLog(LOG_ERR, "Fehler beim oeffnen %s", device);
		result="\0";
		*resultLen=0;
		return(0);
	}
	
	cmdPtr=fopen(filename,"r");
	if (!cmdPtr) {
		VCLog(LOG_ERR, "Kann Cmd File %s nicht oeffnen", filename);
		result="\0";
		*resultLen=0;
		return(0);
	} 
	VCLog(LOG_INFO, "Lese Cmd File %s", filename);
	/* Queue leeren */
	vcontrol_semget();
        tcflush(fd,TCIOFLUSH);
	vcontrol_semrelease();
	while(fgets(line,MAXBUF-1,cmdPtr)) {
		/* \n verdampfen */
		line[strlen(line)-1]='\0';
		bzero(recvBuf,sizeof(recvBuf));
		vcontrol_semget();
		count=execCmd(line,fd,recvBuf,sizeof(recvBuf));
		vcontrol_semrelease();
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
			sprintf(string,"%02X ",byte);
			strcat(buffer,string);
			if (n >= MAXBUF-3)
				break;
		}
		if (count -1) {
			/* timeout */
		}
		if (count) {
			VCLog(LOG_INFO, "Empfangen: %s", buffer);
		}
			
	}
	close(fd);
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
	char string[1000];
	
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
			VCLog(LOG_ERR, "Fehler Erzeugung mkstemp");
			return(0);
		}
	}
	filePtr=fopen(tmpfile,"w+");
	if (!filePtr) {
		VCLog(LOG_ERR, "Kann Tmp File %s nicht anlegen", tmpfile);
		return(0);
	} 
	VCLog(LOG_INFO, "Raw Modus: Temp Datei: %s", tmpfile);
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
				sprintf(string,"Result: %s\n",buffer);
				Writen(socketfd,string,strlen(string));
			}
			remove(tmpfile);	
			return(1);
		}
		VCLog(LOG_INFO, "Raw: Gelesen: %s", readBuf);
		/*
		int n;
		if ((n=fputs(readBuf,filePtr))== 0) {
		*/
		if (fputs(readBuf,filePtr)== EOF) {
			VCLog(LOG_ERR, "Fehler beim schreiben tmp Datei");
		}
		else {
			/* debug stuff */
		}
			
	}	
	return 0;			// is this correct?
}

int interactive(int socketfd,char *device )
{
	char readBuf[1000];
	char *readPtr;
	char prompt[]=PROMPT;
	char bye[]=BYE;
	char string[1000];
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

	VCLog(LOG_DEBUG, "vcontrol::interactive(socketfd=%d, device=%s)", socketfd, device);

	Writen(socketfd,prompt,strlen(prompt));
	bzero(readBuf,sizeof(readBuf));
	
	while((rcount=Readline(socketfd,readBuf,sizeof(readBuf)))) {
				
		sendErrMsg(socketfd);
		/* Steuerzeichen verdampfen */
		/*readPtr=readBuf+strlen(readBuf); **/
		readPtr=readBuf+rcount;
		while(iscntrl(*readPtr))
			*readPtr--='\0';
		VCLog(LOG_INFO, "Befehl: %s", readBuf);

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
			close(fd);
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
				sprintf(string,"XMLFile %s neu geladen\n",xmlfile);
				Writen(socketfd,string,strlen(string));
				/* falls wir einen Vater haben (Daemon Mode) bekommt er ein sighup */
				if (makeDaemon) {
					kill(getppid(),SIGHUP);
				} 
			}
			else {
				bzero(string,sizeof(string));
				sprintf(string,"Laden von XMLFile %s gescheitert, nutze alte Konfig\n",xmlfile);
				Writen(socketfd,string,strlen(string));
			}

		}
		else if(strstr(readBuf,"raw")==readBuf) {
			rawModus(socketfd,device);
		}
		else if(strstr(readBuf,"close")==readBuf) {
			close(fd);
			sprintf(string,"%s geschlossen\n",device);
			Writen(socketfd,string,strlen(string));
			fd=-1;
		}
/* 		else if(strstr(readBuf,"open")==readBuf) {
			if ((fd<0) && (fd=openDevice(device))== -1) {
				sprintf(string,"Fehler beim oeffnen %s",device);
				logIT(LOG_ERR,string);
			}
			sprintf(string,"%s geoeffnet\n",device);
			Writen(socketfd,string,strlen(string));
		}
*/
		else if(strstr(readBuf,"commands")==readBuf) {
			cPtr=cfgPtr->devPtr->cmdPtr; 
			while (cPtr) {
				if (cPtr->addr) {
					bzero(string,sizeof(string));
					sprintf(string,"%s: %s\n",cPtr->name,cPtr->description);
					Writen(socketfd,string,strlen(string));
				}
				cPtr=cPtr->next;
			}

		}
		else if(strstr(readBuf,"protocol")==readBuf) {
			bzero(string,sizeof(string));
			sprintf(string,"%s\n",cfgPtr->devPtr->protoPtr->name);
			Writen(socketfd,string,strlen(string));
		}
		else if(strstr(readBuf,"device")==readBuf) {
			bzero(string,sizeof(string));
			sprintf(string,"%s (ID=%s) (Protocol=%s)\n",cfgPtr->devPtr->name,
							cfgPtr->devPtr->id,
							cfgPtr->devPtr->protoPtr->name);
			Writen(socketfd,string,strlen(string));
		}
		else if(strstr(readBuf,"version")==readBuf) {
			bzero(string,sizeof(string));
			sprintf(string,"Version: %s\n",VERSION);
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
					VCLog(LOG_ERR, "Kein Hex string: %s", para);
					sendErrMsg(socketfd);
					if (!Writen(socketfd,prompt,strlen(prompt))) {
						sendErrMsg(socketfd);
						close(fd);
						return(0);
					}
					continue;
				}
				/* falls sendLen > als len der Befehls, nehmen wir len */
				if (sendLen > cPtr->len) {
					VCLog(LOG_WARNING,"Laenge des Hex Strings > Sendelaenge des Befehls, sende nur %d Byte", cPtr->len);
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
			if ((fd<0) && (fd=openDevice(device))== -1) {
				VCLog(LOG_ERR, "Fehler beim oeffnen %s", device);
				sendErrMsg(socketfd);
				if (!Writen(socketfd,prompt,strlen(prompt))) {
					sendErrMsg(socketfd);
					close(fd);
					return(0);
				}
				continue;
			}

			vcontrol_semget();

			/* falls ein Pre-Kommando definiert wurde, fuehren wir dies zuerst aus */
			if (cPtr->precmd &&(pcPtr=getCommandNode(cfgPtr->devPtr->cmdPtr,cPtr->precmd))) {
				VCLog(LOG_INFO, "Fuehre Pre Kommando %s aus", cPtr->precmd);

				if (execByteCode(pcPtr->cmpPtr,fd,pRecvBuf,sizeof(pRecvBuf),sendBuf,sendLen,1,pcPtr->bit,pcPtr->retry,pRecvBuf,pcPtr->recvTimeout)==-1) {	
				  vcontrol_semrelease();
				  VCLog(LOG_ERR, "Fehler beim ausfuehren von %s", readBuf);
				  sendErrMsg(socketfd);
				  break;
				}
				else {
					bzero(buffer,sizeof(buffer));
					char2hex(buffer,pRecvBuf,pcPtr->len);
					VCLog(LOG_INFO, "Ergebnis Pre-Kommand: %s", buffer);
				}
	
					
			}
				
			/* wir fuehren den Bytecode aus, 
			       -1 -> Fehler  
				0 -> Formaterierter String
				n -> Bytes in Rohform */

			count=execByteCode(cPtr->cmpPtr,fd,recvBuf,sizeof(recvBuf),sendBuf,sendLen,noUnit,cPtr->bit,cPtr->retry,pRecvBuf,cPtr->recvTimeout);
			vcontrol_semrelease();

			if (count==-1) {	
				VCLog(LOG_ERR, "Fehler beim ausfuehren von %s", readBuf);
				sendErrMsg(socketfd);
			}
			else if (*recvBuf && (count==0)) { /* Unit gewandelt */
				
			  VCLog(LOG_INFO,"%s", recvBuf);
			  sprintf(string,"%s\n",recvBuf);
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
					sprintf(string,"%02X ",byte);
					strcat(buffer,string);
					if (n >= MAXBUF-3)
						break;
				}
				if (count) {
					sprintf(string,"%s\n",buffer);
					Writen(socketfd,string,strlen(string));
					VCLog(LOG_INFO, "Empfangen: %s", buffer);
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
				sprintf(string,"%s: %s\n",cPtr->name,cPtr->send);
				Writen(socketfd,string,strlen(string));
				/* Error String definiert */
				char buf[MAXBUF];
				bzero(buf,sizeof(buf));
				if (cPtr->errStr && char2hex(buf,cPtr->errStr,cPtr->len)) {
					sprintf(string,"\tError bei (Hex): %s",buf);
					Writen(socketfd,string,strlen(string));
				}
				/* recvTimeout ?*/
				if (cPtr->recvTimeout){
					sprintf(string,"\tRECV Timeout: %d ms\n",cPtr->recvTimeout);
					Writen(socketfd,string,strlen(string));
				}
				/* Retry definiert ? */
				if (cPtr->retry) {
					sprintf(string,"\tRetry: %d\n",cPtr->retry);
					Writen(socketfd,string,strlen(string));
				}
				/* Ist Bit definiert ?*/
				if (cPtr->bit > 0) {
					sprintf(string,"\tBit (BP): %d\n",cPtr->bit);
					Writen(socketfd,string,strlen(string));
				}
				/* Pre-Command definiert ?*/
				if (cPtr->precmd){
					sprintf(string,"\tPre-Kommando (P0-P9): %s\n",cPtr->precmd);
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

						sprintf(string,"\tUnit: %s (%s)\n\t  Type: %s\n\t  Get-Calc: %s\n\t  Set-Calc: %s\n\t Einheit: %s\n",
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
								sprintf(string,"\t  Enum Bytes:%s Text:%s\n",dummy,ePtr->text);
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
				sprintf(string,"ERR: command %s unbekannt\n",readPtr);
				Writen(socketfd,string,strlen(string));
			}
		} 
		else if(*readBuf) { 
			if (!Writen(socketfd,UNKNOWN,strlen(UNKNOWN))) { 
				sendErrMsg(socketfd);
				close(fd);
				return(0);
			}
		
		}
		bzero(string,sizeof(string));
		sendErrMsg(socketfd);
		sprintf(string,"%s",prompt); //,readBuf);  // is this needed? what does it do?
		if (!Writen(socketfd,prompt,strlen(prompt))) {
			sendErrMsg(socketfd);
			close(fd);
			return(0);
		}
		bzero(readBuf,sizeof(readBuf));
	}
	sendErrMsg(socketfd);
	close(fd);
	return(0);
} 

static void sigPipeHandler(int signo) {
	VCLog(LOG_ERR, "SIGPIPE empfangen");
}

static void sigHupHandler(int signo) {
	VCLog(LOG_NOTICE, "SIGHUP empfangen - Konfiguration wird neu eingelesen");
	reloadConfig();
}


/* hier gehts los */

int main(int argc,char* argv[])  {

	/* Auswertung der Kommandozeilenschalter */
	int c;
	char device[200]="\0";
	char cmdfile[200]="\0";
	char logfile[200]="\0";
	int useSyslog=0;	
	int debug=0;	
	int cmdOK=0;
	int tcpport=0;
	int simuOut=0;
	char string[1000];

	/* wir loggen aus stdout, bis die log Optinen richtig gesetzt sind */
	while (--argc > 0 && (*++argv)[0] == '-') {
			c=*++argv[0];
			switch (c) {
			case 'd':
				/* Option -d, serielles Device */
				if (!--argc) 
					usage();
				++argv;
				strncpy(device,argv[0],sizeof(device));
				break;
			case 'c':
				if (!--argc) 
					usage();
				++argv;
				strncpy(cmdfile,argv[0],sizeof(cmdfile));
				break;
			case 'x':
				if (!--argc) 
					usage();
				++argv;
				strncpy(xmlfile,argv[0],sizeof(xmlfile));
				cmdOK=1;
				break;
			case 'l':
				if (!--argc) 
					usage();
				++argv;
				strncpy(logfile,argv[0],sizeof(logfile));
				break;
			case 's':
				useSyslog=1;
				break;
			case 'p':
				if (!--argc)
                                        usage();
                                ++argv;
                                strncpy(string,argv[0],sizeof(device));
				tcpport=atoi(string);
				break;
			case 'n':
				makeDaemon=0;
				break;
			case 'i':
				simuOut=1;
				break;
			case 'g':
				debug=1;
				break;
			default:
				usage();
			}
	}	
	initLog(useSyslog,logfile,debug);

	if (!parseXMLFile(xmlfile)){
		fprintf(stderr,"Fehler beim Laden von %s, terminiere!\n", xmlfile); 
		VCLog(LOG_EMERG, "Fehler beim Laden von %s, terminiere!", xmlfile); 
		exit(1);
	}


	/* es wurden die beiden globalen Variablen cfgPtr und protoPtr gefuellt */
	if (cfgPtr) { 
		if (!tcpport)
			tcpport=cfgPtr->port;
		if (*device == '\0')
			strcpy(device,cfgPtr->tty);
		if (*logfile=='\0')
			strcpy(logfile,cfgPtr->logfile);
		if (!useSyslog)
			useSyslog=cfgPtr->syslog;
		if (!debug)
			debug=cfgPtr->debug;
	}

/*	if (!cmdOK) {
		usage();
	}
*/
	if (!initLog(useSyslog,logfile,debug)) 
		exit(1);

	VCLog(LOG_NOTICE, "Start vcontrold version %s", VERSION);

	if (signal(SIGHUP,sigHupHandler)== SIG_ERR) {
		VCLog(LOG_EMERG,"Fehler beim Signalhandling SIGHUP");
		exit(1);
	}
		

	/* falls -i angegeben wurde, loggen wir die Befehle im Simulator INI Format */
	if (simuOut) {
		char file[100];
		sprintf(file,INIOUTFILE,cfgPtr->devID);
		if (!(iniFD=fopen(file,"w"))) {
			VCLog(LOG_ERR, "Konnte Simulator INI File %s nicht anlegen", file);
		}
		fprintf(iniFD,"[DATA]\n");
	}	

	/* die Macros werden ersetzt und die zu sendenden Strings in Bytecode gewandelt */
	compileCommand(devPtr,uPtr);

	char result[MAXBUF];
	int resultLen=sizeof(result);
	int sid;

	if (tcpport) {
		if (makeDaemon) { 
			int pid;
			/* etwas Siganl Handling */
			if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
				VCLog(LOG_ERR, "Fehler beim Signalhandling SIGCHLD");
			}
			
			pid=fork();
			if (pid <0) {
			  VCLog(LOG_EMERG, "fork fehlgeschlagen (%d)", pid);
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
				VCLog(LOG_EMERG, "setsid fehlgeschlagen");
				exit(1);
			}
			if (chdir("/") <0) {
				VCLog(LOG_EMERG, "chdir / fehlgeschlagen");
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
				VCLog(LOG_EMERG, "Signal error");
				exit(1);
			}
			if (sockfd>=0) {
				/* Socket hat fd zurueckgegeben, den Rest machen wir in interactive */
				interactive(sockfd,device);
				closeSocket(sockfd);
				setDebugFD(-1);
				if (makeDaemon) {
					VCLog(LOG_INFO, "Child Prozess pid:%d beendet", getpid());
					exit(0); /* das Kind verabschiedet sich */
				}
			}
			else {
				VCLog(LOG_ERR, "Fehler bei Verbindungsaufbau");
			}
		}
	}
	else {
	  vcontrol_seminit();

	  if (*cmdfile)
	    readCmdFile(cmdfile,result,&resultLen,device);

	  vcontrol_semfree();
	}

	VCLog(LOG_NOTICE, "vcontrold beendet");
	
	return 0;
}
