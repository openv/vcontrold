/*
 * Vito-Control Client 
 *  */
/* $Id: vclient.c 24 2008-03-18 21:20:31Z marcust $ */
#include <stdlib.h>
#include <stdio.h> 
#include <errno.h>
#include <signal.h>  
#include <syslog.h> 
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/ioctl.h>

#include "common.h"
#include "socket.h"
#include "io.h"
#include "client.h"


#define VERSION "0.3alpha"


void usage() {
	printf("usage: vclient -h <ip:port> [-c <command1,command2,..>] [-f <commandfile>] [-s <csv-Datei>] [-t <Template-Datei>] [-o <outpout Datei> [-x exec-Datei>] [-k] [-m] [-v]\n\n\
\t-h : IP:Port des vcontrold\n\
\t-c: Liste von auszufuehrenden Kommandos, durch Komma getrennt\n\
\t-f: Optional Datei mit Kommandos, pro Zeile ein Kommando\n\
\t-s: Ausgabe des Ergebnisses im CSV Format zur Weiterverarbeitung\n\
\t-t: Template, Variablen werden mit zurueckgelieferten Werten ersetzt.\n\
\t-o Output, der stdout Output wird in die angegebene Datei geschrieben\n\
\t-x Das umgewandelte Template (-t) wird in die angegebene Datei geschrieben und anschliessend ausgefuehrt.\n\
\t-m Munin Datalogger kompatibles Format; Einheiten und Details zu Fehler gehen verloren.\n\
\t-k CactI Datalogger kompatibles Format; Einheiten und Details zu Fehler gehen verloren.\n\
\t-v: Verbose Modus zum testen\n"); 
	
	exit(1);
}

/* hier gehts los */

int
main(int argc,char* argv[])  {

	/* Auswertung der Kommandozeilenschalter */
	char c;
	char host[200] = "";
	char commands[400] = "";
	char cmdfile[MAXPATHLEN] = "";
	char csvfile[MAXPATHLEN] = "";
	char tmplfile[MAXPATHLEN] = "";
	char outfile[MAXPATHLEN] = "";
	char string[1000] = "";
	char result[1000] = "";
	int sockfd;
	char dummylog[]="\0";
	short verbose=0;
	short munin=0;
	short cacti=0;
	short execMe=0;
	trPtr resPtr;
	FILE *filePtr;
	FILE *ofilePtr;
	
/* ToDo: vheat(2013-10-05):
 * derzeitige Implementierung crasht, wenn zwei Optionen ohne Parameter auf einander folgen
 * zB: "vclient -h 1..0 -c xyz -v -k"
 * Es wird ein String erwartet wie bei -h oder -c, der Bindestrich wird nicht erwartet.
 *
 * Fix sollte sein: Umbau auf getopt(), um die Parameter zu lesen.
 */
	while (--argc > 0 && (*++argv)[0] == '-') {
			c=*++argv[0];
			switch (c) {
			case 'h':
				if (!--argc) 
					usage();
				++argv;
				strncpy(host,argv[0],sizeof(host));
				break;
			case 'c':
				if (!--argc) 
					usage();
				++argv;
				strncpy(commands,argv[0],sizeof(commands));
				break;
			case 'v':
				++argv;
				verbose=1;
				break;
                        case 'm':
                                ++argv;
                                munin=1;
                                break;
                        case 'k':
                                ++argv;
                                cacti=1;
                                break;
			case 'f':
				if (!--argc) 
					usage();
				++argv;
				strncpy(cmdfile,argv[0],sizeof(cmdfile));
				break;
			case 't':
				if (!--argc) 
					usage();
				++argv;
				strncpy(tmplfile,argv[0],sizeof(tmplfile));
				break;
			case 'o':
			case 'x':
				if (!--argc) 
					usage();
				++argv;
				strncpy(outfile,argv[0],sizeof(outfile));
				if (c == 'x')  /* wir merken uns den execute Mode */	
					execMe=1;
				break;
			case 's':
				if (!--argc) 
					usage();
				++argv;
				strncpy(csvfile,argv[0],sizeof(cmdfile));
				break;
			default:
				usage();
			}
	}	
	initLog(0,dummylog,verbose);	
	if (!*commands && (cmdfile[0] == 0))
		usage();
	sockfd=connectServer(host);
	if (sockfd < 0) {
		sprintf(string,"Keine Verbindung zu %s",host);
		logIT(LOG_ERR,string);
		exit(1);
	}
	/* Kommandos direkt angegeben */
	resPtr=NULL;
	if (*commands) { 
		resPtr=sendCmds(sockfd,commands);
	}
	else if (*cmdfile) {
		resPtr=sendCmdFile(sockfd,cmdfile);
	}
	if (!resPtr) {
		logIT(LOG_ERR,"Fehler bei der Server Kommunikation");
		exit(1);
	}
	disconnectServer(sockfd);

	if(*outfile) {
		if (!(ofilePtr=fopen(outfile,"w"))) {
			sprintf(string,"Kann Datei %s nicht anlegen",outfile);
			logIT(LOG_ERR,string);
			exit(1);
		}
		bzero(string,sizeof(string));
		sprintf(string,"Ausgabe Datei %s",outfile);
		logIT(LOG_INFO,string);
	}
	else {
		ofilePtr=fdopen(fileno(stdout),"w");
	}
	
	/* das Ergebnis ist in der Liste resPtr, nun unterscheiden wir die Ausgabe */
	if (*csvfile) {
		/* Kompakt Format mit Semikolon getrennt */
		if (!(filePtr=fopen(csvfile,"a"))) {
			sprintf(string,"Kann Datei %s nicht anlegen",csvfile);
			logIT(LOG_ERR,string);
			exit(1);
		}
		bzero(string,sizeof(string));
		bzero(result,sizeof(result));
		while (resPtr) {
			if (resPtr->err) {
				/* fprintf(stderr,"%s:%s\n",resPtr->cmd,resPtr->err); */
				fprintf(stderr,"%s: server error\n",resPtr->cmd);
				strcat(result,";"); 
				resPtr=resPtr->next;
				continue;
			}
			bzero(string,sizeof(string));
			sprintf(string,"%f;",resPtr->result);
			strncat(result,string,sizeof(result) - strlen(result) - 1);
			resPtr=resPtr->next;
		}
		/*letztes Semikolon verdampfen und \n dran*/
		if (*result) {
			*(result+strlen(result)-1)='\n';
			fputs(result,filePtr);
		}
		fclose(filePtr);
	}
	else if (*tmplfile) { /* Template angegeben*/
		char line[1000];
		char *lptr;
		char *lSptr;
		char *lEptr;
		char varname[20];
		unsigned short count;
		unsigned short idx;
		unsigned short maxIdx;
		trPtr tPtr=resPtr;
		trPtr *idxPtr;
		short varReplaced;

		/* im Array idxPtr werden die einzelnen Ergebnisse ueber den Index referenziert */
		for(count=0;tPtr;tPtr=tPtr->next)
			count++;

		/* wir reservieren uns ein Array mit der passenden Groesse */
		idxPtr=calloc(count,sizeof(tPtr));
		
		maxIdx=count; /* groesster Index in den Variablen */

		count=0;
		tPtr=resPtr;
		while(tPtr) {
			idxPtr[count++]=tPtr;
			tPtr=tPtr->next;
		}		



		if (!(filePtr=fopen(tmplfile,"r"))) {
			sprintf(string,"Kann Template-Datei %s nicht oeffnen",tmplfile);
			logIT(LOG_ERR,string);
			exit(1);
		}
		/*
		Es gibt folgende Variablen zum Ersetzen:
			$Rn: Result (trPtr->raw)
			$n: Float (trPtr->result)
		*/
		while((fgets(line,sizeof(line)-1,filePtr))) {
			sprintf(string,"Tmpl Zeile:%s",line);
			logIT(LOG_INFO,string);
			lSptr=line;
			while((lptr=strchr(lSptr,'$'))) {	
				varReplaced=0;
				if ((lptr>line) && (*(lptr-1)=='\\')) { /* $ ist durch \ ausmaskiert */
					bzero(string,sizeof(string));
					strncpy(string,lSptr,lptr-lSptr-1);	
					fprintf(ofilePtr,"%s%c",string,*lptr);
					lSptr=lptr+1;
					continue;
				}
				lEptr=lptr+1;
				/* wir suchen nun das Ende der Variablen */
				while(isalpha(*lEptr)|| isdigit(*lEptr))
					lEptr++;
				bzero(varname,sizeof(varname));
				strncpy(varname,lptr+1,lEptr-lptr-1);
				sprintf(string,"\tVariable erkannt:%s",varname);
				logIT(LOG_INFO,string);

				/* wir geben schon mal alles bis dahin aus */
				bzero(string,sizeof(string));
				strncpy(string,lSptr,lptr-lSptr);	
				fprintf(ofilePtr,"%s",string);

				/* wir unterscheiden die verschiedenen Variablen */

				/* $Rn */
				if ((strlen(varname)>1) &&
				    (*varname=='R') &&
				    (idx=atoi(varname+1))) {  
					/* Variable R Index dahinter in idx */
					if ((idx-1) < maxIdx) {
						tPtr=idxPtr[idx-1];
						sprintf(string,"%s:%s",tPtr->cmd,tPtr->raw);
						logIT(LOG_INFO,string);
						if (tPtr->raw)
							fprintf(ofilePtr,"%s",tPtr->raw);
					}
					else {
						sprintf(string,"Index der Variable $%s > %d",varname,maxIdx-1);
						logIT(LOG_ERR,string);
					}
				}
				/* $Cn */
				else if ((strlen(varname)>1) &&
				    (*varname=='C') &&
				    (idx=atoi(varname+1))) {  
					/* Variable R Index dahinter in idx */
					if ((idx-1) < maxIdx) {
						tPtr=idxPtr[idx-1];
						sprintf(string,"Kommando: %s",tPtr->cmd);
						logIT(LOG_INFO,string);
						if (tPtr->cmd)
							fprintf(ofilePtr,"%s",tPtr->cmd);
					}
					else {
						sprintf(string,"Index der Variable $%s > %d",varname,maxIdx-1);
						logIT(LOG_ERR,string);
					}
				}
				/* $En */
				else if ((strlen(varname)>1) &&
				    (*varname=='E') &&
				    (idx=atoi(varname+1))) {  
					/* Variable R Index dahinter in idx */
					if ((idx-1) < maxIdx) {
						tPtr=idxPtr[idx-1];
						sprintf(string,"Fehler: %s:%s",tPtr->cmd,tPtr->err);
						logIT(LOG_INFO,string);
						if (tPtr->err)
							fprintf(ofilePtr,"%s",tPtr->err);
					}
					else {
						sprintf(string,"Index der Variable $%s > %d",varname,maxIdx-1);
						logIT(LOG_ERR,string);
					}
				}
				/* $n */
				else if (isdigit(*varname) && (idx=atoi(varname)))  {
					if ((idx-1) < maxIdx) {
						tPtr=idxPtr[idx-1];
						sprintf(string,"%s:%f",tPtr->cmd,tPtr->result);
						logIT(LOG_INFO,string);
						if (tPtr->result)
							fprintf(ofilePtr,"%f",tPtr->result);
					}
					else {
						sprintf(string,"Index der Variable $%s > %d",varname,maxIdx-1);
						logIT(LOG_ERR,string);
					}
				}
				else {
					bzero(string,sizeof(string));
					strncpy(string,lptr,lEptr-lptr);	
					fprintf(ofilePtr,"%s",string);
				}
				lSptr=lEptr;
			}
			fprintf(ofilePtr,"%s",lSptr);
		}
		fclose(filePtr);
		if (*outfile && execMe) { /* Datei ausfuerhbar machen und starten */
			fclose(ofilePtr);
			bzero(string,sizeof(string));
			sprintf(string,"Fuehre Datei %s aus",outfile);
			logIT(LOG_INFO,string);
			if (chmod(outfile,S_IXUSR|S_IRUSR|S_IWUSR)!=0) {
				sprintf(string,"Fehler chmod +x %s",outfile);
				logIT(LOG_ERR,string);
				exit(1);
			}
			short ret;
			if ((ret=system(outfile)) == -1) { 
				sprintf(string,"Fehler system(%s)",outfile);
				logIT(LOG_ERR,string);
				exit(1);
			}
			sprintf(string,"Ret Code: %d",ret);
                        logIT(LOG_INFO,string);
                        exit(ret);
		}
	}
	else if (munin) { /*Munin Format ausgeben*/
		while (resPtr) {
			fprintf(ofilePtr,"%s.value ",resPtr->cmd);
			if (resPtr->err) {
				fprintf(ofilePtr,"U\n");
				resPtr=resPtr->next;
				continue;
			}
			if (resPtr->raw)
				fprintf(ofilePtr,"%f\n",resPtr->result);
			else
				fprintf(ofilePtr,"U\n");				
			resPtr=resPtr->next;
		}
	}	
        else if (cacti) { /*Cacti Format ausgeben*/
		int index = 1;
                while (resPtr) {
                        fprintf(ofilePtr,"v%d:",index);
                        if (resPtr->err) {
                                fprintf(ofilePtr,"U ");
                                resPtr=resPtr->next;
                                index++;
				continue;
                        }
                        if (resPtr->raw)
                                fprintf(ofilePtr,"%f ",resPtr->result);
                        else
                                fprintf(ofilePtr,"U ");                        
                        index++;
			resPtr=resPtr->next;
                }
		fprintf(ofilePtr,"\n");
        }
	else {
		while (resPtr) {
			fprintf(ofilePtr,"%s:\n",resPtr->cmd);
			if (resPtr->err) {
			/*	fprintf(stderr,"%s",resPtr->err); */
				fprintf(stderr,"server error\n");
				resPtr=resPtr->next;
				continue;
			}
			if (resPtr->raw)
				fprintf(ofilePtr,"%s\n",resPtr->raw);
			resPtr=resPtr->next;
		}
	}
	return 0;
}	
