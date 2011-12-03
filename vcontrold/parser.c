/*
 * parser.c, Routinen fuer das Lesen von Kommandos
 *  */
/* $Id: parser.c 34 2008-04-06 19:39:29Z marcust $ */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <ctype.h>

#include "xmlconfig.h"
#include "parser.h"
#include "unit.h"
#include "common.h"
#include "io.h"


/* externe Variablen */
extern FILE *iniFD; /* fuer das Anlegen des Sim. INI Files */


void *getUnit(char *str) {
	/* wir parsen die Eingabe nach einer bekannten Unit*/
	/* und geben einen Zeiger auf die Struktur zurueck */
	return(NULL);
}



int parseLine(char *line,char *hex,int *hexlen,char *uSPtr) {
	int token=0;
	char string[1000];
	if(strstr(line,"WAIT")==line) 
		token=WAIT;
	else if(strstr(line,"SEND BYTES")==line) {
		token=BYTES;
	}
	else if(strstr(line,"SEND")==line) 
		token=SEND;
	else if(strstr(line,"RECV")==line)
		token=RECV;
	else if(strstr(line,"PAUSE")==line)
		token=PAUSE;
	if (token) {
		/* wir parsen den Hex String und liefern ihn gewandelt zurück */
		/* erste Leerstelle suchen */
		char *ptr;
		ptr=strchr(line,' ');
		if (!ptr) {
			sprintf(string,"Parse error, kein Leerzeichen gefunden: %s",line);
			logIT(LOG_ERR,string);
		}
		else {
			/* wir nudeln den Rest der Zeile durch */
			char pString[MAXBUF];
			char *pPtr;
			while (*ptr != '\0') {
				int i;
				pPtr=pString;
				for(i=0;i<MAXBUF;i++)
					pPtr[i]='\0';
				pPtr=pString;
				/* Leerstellen verdampfen */
				int endFound=0;
				while( *++ptr == ' ')  
					if (*ptr == '\0') {
						endFound=1;
						break;
					}
				if (endFound) 
					break;
				/* wir lesen nun die Zeichen ein bis zum naechsten Blank oder Ende */
				while ((*ptr != ' ') && (*ptr != '\0') && (*ptr !='\n') && (*ptr !='\r')) {
					*pPtr++ = *ptr++;
				}
				if (!*pString) 
					break;
				*pPtr='\0';
				++*hexlen;
				/* wenn es sich um Pause oder Receive handelt, behandeln wir den Wert als Integer */
				if (token == PAUSE || token == RECV) {
					*hexlen=atoi(pString);
					/* Bei RECV kann man noch die Unit angeben, in die umgerechnet werden soll */
					if (token == RECV)  {
						pPtr=pString;
						while(*ptr && (isdigit(*ptr)||isspace(*ptr)))
							ptr++;
						strncpy(uSPtr,ptr,sizeof(uSPtr));	
					}
					return(token);
				}
				else if (token == BYTES ) { /* hier folgt noch die Unit */
					pPtr=pString;
					while(*ptr && (isdigit(*ptr)||isspace(*ptr)))
						ptr++;
					strncpy(uSPtr,ptr,sizeof(uSPtr));	
					return(token);
				}
				else  {
					*hex++=hex2chr(pString);
				}
				*pString='\0';
			
			}
		}

	
			 
	}
	return(token);
}

int execByteCode(compilePtr cmpPtr,int fd,char *recvBuf,short recvLen,char *sendBuf,short sendLen,short supressUnit, char bitpos, int retry, char *pRecvPtr,unsigned short recvTimeout) {

	char string[1000];
        char result[MAXBUF];
	char simIn[500];
	char simOut[500];
	short len;
	compilePtr cPtr=cmpPtr;
	unsigned long etime;

/* 	struct timespec t_sLeep; */
/* 	struct timespec t_sleep_rem; */

	bzero(simIn,sizeof(simIn));
	bzero(simOut,sizeof(simOut));
	/* wir wandeln zuerst die zu sendenen Bytes, nicht daas wir mittendrin abbrechen muessen */
	if (!supressUnit) 
		while (cPtr) {
			if (cPtr->token != BYTES) {
				cPtr=cPtr->next;
				continue;
			}
			if (cPtr->uPtr) {
				if(procSetUnit(cPtr->uPtr,sendBuf,&len,bitpos,pRecvPtr)<=0) {
					sprintf(string,"Fehler Unit Wandlung:%s",sendBuf);
					logIT(LOG_ERR,string);
					return(-1);
				}
				if (cPtr->send) { /* wir haben das schon mal gesendet, der Speicher war noch alloziert */
					free(cPtr->send);
					cPtr->send=NULL;
				}
				cPtr->send=calloc(len,sizeof(char));
				cPtr->len=len;
				sendLen=0; /* wir senden nicht den ungewandelten sendBuf */
				memcpy(cPtr->send,sendBuf,len);
			}
			cPtr=cPtr->next;
		}
	do {	
		cPtr=cmpPtr; /* fuer die naechste Runde brauchen wir den Anfang */
		while (cmpPtr) {
			switch(cmpPtr->token) {
			case WAIT:
				if (!waitfor(fd,cmpPtr->send,cmpPtr->len)) {
					logIT(LOG_ERR,"Fehler wait, Abbruch");
					return(-1);
				}
				bzero(string,sizeof(string));
				char2hex(string,cmpPtr->send,cmpPtr->len);
				strcat(simIn,string);
				strcat(simIn," ");
				break;
			case SEND:
				if (!my_send(fd,cmpPtr->send,cmpPtr->len)) {
					logIT(LOG_ERR,"Fehler send, Abbruch");
					return(-1);
				}
				if (iniFD && *simIn && *simOut) { /* wir haben schon was gesendet und empfangen, das geben wir nun aus */
					fprintf(iniFD,"%s= %s\n",simOut,simIn);
					bzero(simOut,sizeof(simOut));
					bzero(simIn,sizeof(simIn));
				}
				bzero(string,sizeof(string));
				char2hex(string,cmpPtr->send,cmpPtr->len);
				strcat(simOut,string);
				strcat(simOut," ");
				break;
			case RECV:
				if (cmpPtr->len > recvLen) {
					sprintf(string,"Recv Buffer zu klein. Ist: %d Soll %d",recvLen,cmpPtr->len);
					logIT(LOG_ERR,string);
					cmpPtr->len=recvLen; /* hoffentlich kommen wir hier nicht hin */
				}	
				etime=0;
				bzero(recvBuf,sizeof(recvBuf));
				if (receive(fd,recvBuf,cmpPtr->len,&etime)<=0) {
					logIT(LOG_ERR,"Fehler recv, Abbruch");
					return(-1);	
				}
				/* falls wir beim empfangen laenger als der Timeout gebraucht haben gehts in die 
				naechste Rune */
				if (recvTimeout && (etime > recvTimeout)) {
				  sprintf(string,"Recv Timeout: %ld ms  > %d ms (Retry: %d)",etime,recvTimeout,(int)(retry-1));
					logIT(LOG_NOTICE,string);
					if (retry <=1) {
						sprintf(string,"Recv Timeout, Abbruch");
						logIT(LOG_ERR,string);
						return(-1);
					}
					goto RETRY;
				}
				
				/*  falls ein errStr definiert ist, schauen wir mal ob das Ergebnis richtig ist */
				if (cmpPtr->errStr && *cmpPtr->errStr) {
					if (memcmp(recvBuf,cmpPtr->errStr,cmpPtr->len)==0) { /* falsche Antwort */
						sprintf(string,"Errstr matched, Ergebnis falsch (Retry:%d)",retry-1);
						logIT(LOG_NOTICE,string);
						if (retry <=1) {
							sprintf(string,"Ergebnis falsch, Abbruch");
							logIT(LOG_ERR,string);
							return(-1);
						}
						goto RETRY;
					}
				}
				bzero(string,sizeof(string));
				char2hex(string,recvBuf,cmpPtr->len);
				strcat(simIn,string);
				strcat(simIn," ");
				/* falls wir eine Unit haben (==uPtr) rechnen wir den 
	 * 			empfangenen Wert um, und geben den umgerechneten Wert auch in uPtr zurueck */
				bzero(result,sizeof(result));
				if (!supressUnit && cmpPtr->uPtr) {
					if (procGetUnit(cmpPtr->uPtr,recvBuf,cmpPtr->len,result,bitpos,pRecvPtr)<=0) {
						sprintf(string,"Fehler Unit Wandlung:%s",result);
						logIT(LOG_ERR,string);
						return(-1);
					}
					strncpy(recvBuf,result,recvLen);
					if (iniFD && *simIn && *simOut) /* wir haben gesendet und empfangen, das geben wir nun aus */
						/* fprintf(iniFD,"%s= %s ;%s\n",simOut,simIn,result); */
						fprintf(iniFD,"%s= %s \n",simOut,simIn);
					return(0); /* 0==geawandelt nach unit */
				}
				if (iniFD && *simIn && *simOut)  /* wir haben gesendet und empfangen, das geben wir nun aus */
					fprintf(iniFD,"%s= %s \n",simOut,simIn);
				return(cmpPtr->len);
				break;
			case PAUSE:
				sprintf(string,"Warte %i ms",cmpPtr->len);
				logIT(LOG_INFO,string);
				usleep(cmpPtr->len * 1000L);
				/* t_sleep.tv_sec=(time_t) cmpPtr->len / 1000;
				t_sleep.tv_nsec=(long) cmpPtr->len * 1000000; 
				if (nanosleep(&t_sleep,&t_sleep_rem)==-1) 
					nanosleep(&t_sleep_rem,NULL);
				*/
				break;
			case BYTES:
				/* wir senden den sendBuffer, der uebergeben wurde */
				/* es fand keine Wandlung statt */
				if (sendLen) {
					if (!my_send(fd,sendBuf,sendLen)) {
						logIT(LOG_ERR,"Fehler send, Abbruch");
						return(-1);
					}
					char2hex(string,sendBuf,sendLen);
					strcat(simOut,string);
					strcat(simOut," ");
				}
				/* es ist eine Einheit definiert soll benutzt werden und wir haben das oben schon gewandelt */
				else if (cmpPtr->len) {
					if (!my_send(fd,cmpPtr->send,cmpPtr->len)) {
						logIT(LOG_ERR,"Fehler send unit Bytes, Abbruch");
						free(cmpPtr->send);
						cmpPtr->send=NULL;
						cmpPtr->len=0;
						return(-1);
					}
					bzero(string,sizeof(string));
					char2hex(string,cmpPtr->send,cmpPtr->len);
					strcat(simOut,string);
					strcat(simOut," ");
/*					free(cmpPtr->send);
					cmpPtr->len=0;
*/
				}
				break;

			default:
				sprintf(string,"unbekanntes Token: %d",cmpPtr->token);
				logIT(LOG_ERR,string);
				return(-1);
			}
			cmpPtr=cmpPtr->next;
		}
		RETRY:
		retry--;
		cmpPtr=cPtr; /* von vorne bitte */
	} while ((cmpPtr->errStr||recvTimeout) && (retry >0));
	return(0);
}

int execCmd(char *cmd,int fd,char *recvBuf, int recvLen) {
	char string[1000];
	char uString[100];
	char *uSPtr=uString;
	sprintf(string,"Execute %s",cmd);
	logIT(LOG_INFO,string);
	/* wir parsen die einzelnen Zeilen */
	char hex[MAXBUF];
	int token;
	int hexlen=0;
	int t;
	unsigned long etime;
	token=parseLine(cmd,hex,&hexlen,uSPtr);
	/* wenn noOpen fuer debug Zwecke gesetzt ist, machen wir hier nichts */
	switch (token) {
		case WAIT:
			if (!waitfor(fd,hex,hexlen)) {
				logIT(LOG_ERR,"Fehler wait, Abbruch");
				return(-1);
			}
			break;
		case SEND:
			if (!my_send(fd,hex,hexlen)) {
				logIT(LOG_ERR,"Fehler send, Abbruch");
				exit(1);
			}
			break;
		case RECV:
			if (hexlen > recvLen) {
				sprintf(string,"Recv Buffer zu klein. Ist: %d Soll %d",recvLen,hexlen);
				logIT(LOG_ERR,string);
				hexlen=recvLen;
			}	
			etime=0;
			if (receive(fd,recvBuf,hexlen,&etime)<=0) {
				logIT(LOG_ERR,"Fehler recv, Abbruch");
				exit(1);
			}
			sprintf(string,"Recv: %ld ms",etime);
			logIT(LOG_INFO,string);
			/* falls wir eine Unit haben (==uPtr) rechnen wir den 
 * 			empfangenen Wert um, und geben den umgerechneten Wert auch in uPtr zurueck */
			return(hexlen);
			break;
		case PAUSE:
			t=(int) hexlen/1000;
			sprintf(string,"Warte %i s",t);
			logIT(LOG_INFO,string);
			sleep(t);
			break;

		default:
			sprintf(string,"unbekannter Befehl: %s",cmd);
				logIT(LOG_INFO,string);
	}
	return(0);
}

compilePtr newCompileNode(compilePtr ptr) {
        compilePtr nptr;
        if (ptr && ptr->next) {
                return(newCompileNode(ptr->next));
        }
        nptr=calloc(1,sizeof(Compile));
        if (!nptr) {
                fprintf(stderr,"malloc gescheitert\n");
                exit(1);
        }
        if (ptr)
                ptr->next=nptr;
        nptr->token=0;
        nptr->len=0;
	nptr->uPtr=NULL;
	nptr->next=NULL;
        return(nptr);
}

void removeCompileList(compilePtr ptr) {
	if (ptr && ptr->next)
		removeCompileList(ptr->next);
	if (ptr) {
		free(ptr->send);
		free(ptr);
	}
}


int expand(commandPtr cPtr,protocolPtr pPtr) {

	/* Rekursion */
	if (!cPtr)
		return(0);
	if (cPtr->next)
		expand(cPtr->next,pPtr);


	/* falls keine Adresse gesetzt ist machen wir nichts */
	if (!cPtr->addr)
		return(0);


	char eString[2000];
	char *ePtr=eString;;
	char var[100];
	char name[100];
	char *ptr,*bptr;
	macroPtr mFPtr;
	char string[1000];
	char *sendPtr,*sendStartPtr;
	char *tmpPtr;

	icmdPtr iPtr;

	/* send des Kommand Pointers muss gebaut werden 
	1. Suche Kommando pcmd bei den Kommandos des Protokolls */
	if (!(iPtr= (icmdPtr) getIcmdNode( pPtr->icPtr, cPtr->pcmd))) {
		sprintf(string,"Protokoll Kommando %s (bei %s) nicht definiert",cPtr->pcmd,cPtr->name);
		logIT(LOG_ERR,string);
		exit(3);
	}
/*	2. Parse die zeile und ersetze die Variablen durch Werte in cPtr */
	sendPtr=iPtr->send;
	sendStartPtr=iPtr->send;
	if (!sendPtr) 
		return(0);
	sprintf(string,"protocmd Zeile: %s",sendPtr);
	logIT(LOG_INFO,string);
	bzero(eString,sizeof(eString));
	cPtr->retry=iPtr->retry; /* wir uebernehmen den Retry Wert aus des Protokoll Kommandos */	
	cPtr->recvTimeout=iPtr->recvTimeout; /* dito fuer den Receive Timeout */ 
	do {
		ptr=sendPtr;
		while(*ptr++) {
			if ((*ptr=='$')||(*ptr=='\0')) 
				break;
		}
		bptr=ptr;
		while(*bptr++) {
			if ((*bptr==' ')||(*bptr==';')|| (*bptr=='\0')) 
				break;
		}
		/* kopiere nicht umgewandelten String*/
		strncpy(ePtr,sendPtr,ptr-sendPtr);
		ePtr+=ptr-sendPtr;
		bzero(var,sizeof(var));
		strncpy(var,ptr+1,bptr-ptr-1);
/*		sprintf(string,"   Var: %s",var);
		logIT(LOG_INFO,string); */
		if(*var) { /* Haben wir uerbhaupt Variablen zu expandieren */ 
			if (strstr(var,"addr")==var) {
				/* immer zwei Byte zusammen */
				int i;
				bzero(string,sizeof(string));
				for(i=0;i<strlen(cPtr->addr)-1;i+=2) {
					strncpy(ePtr,cPtr->addr+i,2);
					ePtr+=2;
					*ePtr++=' ';
				}
				ePtr--;
			}
			else if (strstr(var,"len")==var) {
				sprintf(string,"%d",cPtr->len);
				strncpy(ePtr,string,strlen(string));
				ePtr+=strlen(string);
			}
			else if (strstr(var,"hexlen")==var) {
				sprintf(string,"%02X",cPtr->len);
				strncpy(ePtr,string,strlen(string));
				ePtr+=strlen(string);
			}
			else if (strstr(var,"unit")==var) {
				if (cPtr->unit) {
					strncpy(ePtr,cPtr->unit,strlen(cPtr->unit));
					ePtr+=strlen(cPtr->unit);
				}
			}
			else {
				sprintf(string,"Variable %s unbekannt",var);
				logIT(LOG_ERR,string);
				exit(3);
			}
		}
		sendPtr=bptr;
	} while(*sendPtr);
	sprintf (string,"  Nach Ersetzung: %s",eString);
	logIT(LOG_INFO,string);
	tmpPtr=calloc(strlen(eString)+1,sizeof(char));
	strcpy(tmpPtr,eString);
	
/*	3. Expandiere die Zeile dann wie gehabt */
	
	sendPtr=tmpPtr;
	sendStartPtr=sendPtr;

	/* wir suchen nach woertern und schauen, ob es die als Macros gibt */

	bzero(eString,sizeof(eString));
	ePtr=eString;
	do {
		ptr=sendPtr;
		while(*ptr++) {
			if ((*ptr==' ')||(*ptr==';')|| (*ptr=='\0')) 
				break;
		}
		bzero(name,sizeof(name));
		strncpy(name,sendPtr,ptr-sendPtr);
		if ((mFPtr=getMacroNode(pPtr->mPtr,name))) {
			strncpy(ePtr,mFPtr->command,strlen(mFPtr->command));
			ePtr+=strlen(mFPtr->command);
			*ePtr++=*ptr; 
		}
		else {
			strncpy(ePtr,sendPtr,ptr-sendPtr+1);
			ePtr+=ptr-sendPtr+1;
		}
		sendPtr=ptr+1;
	} while(*sendPtr);
	free(tmpPtr);
	sprintf (string,"   nach EXPAND:%s",eString);
	logIT(LOG_INFO,string);
	if (!(cPtr->send=calloc(strlen(eString)+1,sizeof(char)))) {
			logIT(LOG_ERR,"calloc gescheitert");
			exit(1);
	}
	strcpy(cPtr->send,eString);
	return(1);
}

compilePtr buildByteCode(commandPtr cPtr,unitPtr uPtr) {
	/* Rekursion */
	if (!cPtr)
		return(0);
	if (cPtr->next)
		buildByteCode(cPtr->next,uPtr);

	/* falls keine Adresse gesetzt ist machen wir nichts */
	if (!cPtr->addr)
		return(0);

	char eString[2000];
	char cmd[200];
	char *ptr;

	char hex[MAXBUF];
	int hexlen;
	int token;
	char uString[100];
	char *uSPtr=uString;
	char string[1000];

	compilePtr cmpPtr,cmpStartPtr;

	
	cmpStartPtr=NULL;
	
	
	char *sendPtr,*sendStartPtr;
	sendPtr=cPtr->send;
	sendStartPtr=cPtr->send;
	if (!sendPtr) /* hier gibt es nichts zu tun */
		return(0);
	sprintf(string,"BuildByteCode:%s",sendPtr);
	logIT(LOG_INFO,string);
	bzero(eString,sizeof(eString));
	do {
		ptr=sendPtr;
		while(*ptr++) {
			if ((*ptr=='\0')||(*ptr==';')) {
				break;
			}
		}
		bzero(cmd,sizeof(cmd));
		strncpy(cmd,sendPtr,ptr-sendPtr);
		hexlen=0;
		bzero(uSPtr,sizeof(uString));
		token=parseLine(cmd,hex,&hexlen,uSPtr);
		sprintf(string,"\t\tToken: %d Hexlen:%d, Unit: %s",token,hexlen,uSPtr);
		logIT(LOG_INFO,string);
		cmpPtr=newCompileNode(cmpStartPtr);
		if(!cmpStartPtr)
			cmpStartPtr=cmpPtr;
		cmpPtr->token=token;
		cmpPtr->len=hexlen;
		cmpPtr->errStr=cPtr->errStr;
		cmpPtr->send=calloc(hexlen,sizeof(char));
		memcpy(cmpPtr->send,hex,hexlen);
		if (*uSPtr && !(cmpPtr->uPtr=getUnitNode(uPtr,uSPtr))) {
			sprintf(string,"Unit %s nicht definiert",uSPtr);
			logIT(LOG_ERR,string);
			exit(3);
		}
				
		sendPtr+=strlen(cmd)+1;
	} while(*sendPtr);
	cPtr->cmpPtr=cmpStartPtr;
	return(cmpStartPtr);
}
void compileCommand(devicePtr dPtr,unitPtr uPtr) {
	
	if (!dPtr)
		return;
	if (dPtr->next)
		compileCommand(dPtr->next,uPtr);
	
	char string[1000];

	
	sprintf(string,"Expandiere Kommandos fuer Device %s", dPtr->id);
	logIT(LOG_INFO,string);
	expand(dPtr->cmdPtr,dPtr->protoPtr);
	buildByteCode(dPtr->cmdPtr,uPtr); 

	
}

