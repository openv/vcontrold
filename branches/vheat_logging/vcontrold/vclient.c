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
#include <getopt.h>

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


#ifndef VERSION
#define VERSION "0.3alpha"
#endif

void usage() {
	printf("usage: vclient -h <ip:port> [-c <command1,command2,..>] [-f <commandfile>] [-s <csv-Datei>] [-t <Template-Datei>] [-o <outpout Datei> [-x exec-Datei>] [-k] [-m] [-v]\n\n\
\t-h|--host\t<IP>:<Port> des vcontrold\n\
\t-c|--command\tListe von auszufuehrenden Kommandos, durch Komma getrennt\n\
\t-f|--commandfile\tOptional Datei mit Kommandos, pro Zeile ein Kommando\n\
\t-s|--cvsfile\tAusgabe des Ergebnisses im CSV Format zur Weiterverarbeitung\n\
\t-t|--template\tTemplate, Variablen werden mit zurueckgelieferten Werten ersetzt.\n\
\t-o|--output\tOutput, der stdout Output wird in die angegebene Datei geschrieben\n\
\t-x|--execute\tDas umgewandelte Template (-t) wird in die angegebene Datei geschrieben und anschliessend ausgefuehrt.\n\
\t-m|--munin\tMunin Datalogger kompatibles Format; Einheiten und Details zu Fehler gehen verloren.\n\
\t-k|--cacti\tCactI Datalogger kompatibles Format; Einheiten und Details zu Fehler gehen verloren.\n\
\t-v|--verbose\tVerbose Modus zum testen\n");
	
	exit(1);
}

/* hier gehts los */

int
main(int argc,char* argv[])  {

	/* Auswertung der Kommandozeilenschalter */
	char host[256] = "";
	char commands[512] = "";
	char cmdfile[MAXPATHLEN] = "";
	char csvfile[MAXPATHLEN] = "";
	char tmplfile[MAXPATHLEN] = "";
	char outfile[MAXPATHLEN] = "";
	char string[1024] = "";
	char result[1024] = "";
	int sockfd;
	char dummylog[]="\0";
	int opt;
	static int verbose=0;
	static int munin=0;
	static int cacti=0;
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

	while (1)
	{
		static struct option long_options[] =
		{
			
			{"host",		required_argument,	0, 'h'},
			{"command",		required_argument,	0, 'c'},
			{"commandfile",	required_argument,	0, 'f'},
			{"csvfile",		required_argument,	0, 's'},
			{"template",	required_argument,	0, 't'},
			{"output",		required_argument,	0, 'o'},
			{"execute",		required_argument,	0, 'x'},
			{"verbose",		no_argument,		&verbose, 1},
			{"munin",		no_argument,		&munin, 1},
			{"cacti",		no_argument,		&cacti, 1},
			{0,0,0,0}
		};
		/* getopt_long stores the option index here. */
		int option_index = 0;
		opt = getopt_long (argc, argv, "c:f:h:kmo:s:t:vx:",
						   long_options, &option_index);
		
		/* Detect the end of the options. */
		if (opt == -1)
			break;
		
		switch (opt) {
			case 0:
				/* If this option sets a flag, we do nothing for now */
				if (long_options[option_index].flag != 0)
					break;
				printf("option %s", long_options[option_index].name);
				if (optarg)
					printf(" with arg %s", optarg);
				printf("\n");
				break;

			case 'v':
				puts ("option -v\n");
				verbose = 1;
				break;
				
			case 'm':
				if (verbose)
					puts ("option -m\n");
				munin = 1;
				break;
				
			case 'k':
				if (verbose)
					puts ("option -k\n");
				cacti = 1;
				break;
				
			case 'h':
				if (verbose)
					printf ("option -h with value `%s'\n", optarg);
				strncpy(host, optarg, sizeof(host));
				break;
				
			case 'c':
				if (verbose) {
					printf ("option -c with value `%s'\n", optarg);
					printf ("sizeof optarg:%lu, strlen:%lu, sizeof commands:%lu, strlen:%lu,  [%s]\n", sizeof(optarg), strlen(optarg),sizeof(commands),strlen(commands), commands);
				}
				if (strlen(commands)==0) {
					strncpy(commands, optarg, sizeof(commands));
				} else {
					if (strlen(optarg) + 2 > sizeof(commands) - strlen(commands)) {
						fprintf(stderr, "too much commands\n");
						break;
					}
					strncat(commands, ",", 1);
					strncat(commands, optarg, sizeof(commands) - strlen(commands) - 2);
				}
				break;
				
			case 'f':
				if (verbose)
					printf ("option -f with value `%s'\n", optarg);
				strncpy(cmdfile, optarg, sizeof(cmdfile));
				break;
				
			case 's':
				if (verbose)
					printf ("option -s with value `%s'\n", optarg);
				break;
				
			case 't':
				if (verbose)
					printf ("option -t with value `%s'\n", optarg);
				strncpy(tmplfile, optarg, sizeof(tmplfile));
				break;
				
			case 'o':
			case 'x':
				if (verbose)
					printf ("option -%c with value `%s'\n", opt, optarg);
				strncpy(outfile, optarg, sizeof(outfile));
				if (opt == 'x')
					execMe = 1;
				break;
				
			case '?':
				/* getopt_long already printed an error message. */
				usage();
				break;
 				
			default:
				abort();
		}
	}
	
	/* Collect any remaining command line arguments (not options).
	 * and use the as commands like for the -c option.
	 */
	if (optind < argc)
    {
		if (verbose) {
			printf ("non-option ARGV-elements: ");
		}
		while (optind < argc) {
			printf ("%s ", argv[optind]);
			if (strlen(commands)==0) {
				strncpy(commands, argv[optind], sizeof(commands));
			} else {
				if (strlen(argv[optind]) + 2 > sizeof(commands) - strlen(commands)) {
					fprintf(stderr, "Kommandoliste zu lang\n");
					optind++;
					break;
				}
				strncat(commands, ",", 1);
				strncat(commands, argv[optind], sizeof(commands) - strlen(commands) - 2);
			}
			optind++;
		}
		putchar ('\n');
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
						else
							fprintf(ofilePtr,"OK");

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
						//if (tPtr->result)
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
