/* client.c, Client Routinen fuer vcontrold Abfragen */
/* $Id: client.c 17 2008-03-09 11:12:25Z marcust $ */

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>

#include "client.h"
#include "prompt.h"
#include "common.h"
#include "socket.h"

static void sig_alrm(int);
static jmp_buf  env_alrm;

/* Deklarationen */
int sendTrList(int sockfd, trPtr ptr); 


trPtr newTrNode(trPtr ptr) {

	trPtr nptr;
	if (ptr && ptr->next) {
		return(newTrNode(ptr->next));
	}
	nptr=calloc(1,sizeof(*ptr));
	if (!nptr) {
		fprintf(stderr,"malloc gescheitert\n");
		exit(1);
	}
	if (ptr)
		ptr->next=nptr;
	return(nptr);
}


ssize_t recvSync(int fd,char *wait,char **recv) {
	char *string;
	char *rptr;
	char *pptr;
	char c;
	ssize_t count;
	int rcount=1;
	if (signal(SIGALRM, sig_alrm) == SIG_ERR)
		logIT(LOG_ERR,"SIGALRM error");
	if(setjmp(env_alrm) !=0) {
		string=calloc(1000,sizeof(char));
		snprintf(string, 1000-10 ,"timeout wait:%s",wait);
		logIT(LOG_ERR,string);
		free(string);
		return(-1);
	}
	alarm(CL_TIMEOUT);
	if (!(*recv=calloc(ALLOCSIZE,sizeof(char)))) {
			logIT(LOG_ERR,"Fehler calloc");
			exit(1);
	}
	rptr=*recv;
	while((count=readn(fd,&c,1))) {
		alarm(0);
		if (count<0) 
			continue;
		*rptr++=c;
		if(!((rptr-*recv+1)%ALLOCSIZE)) {
			if (realloc(*recv,ALLOCSIZE * sizeof(char) *  ++rcount)==NULL) { 
				logIT(LOG_ERR,"Fehler realloc");
				exit(1);
			}
		}
		if ((pptr=strstr(*recv,wait))) {
			*pptr='\0';
			string=calloc(strlen(*recv)+100,sizeof(char));
			snprintf(string, sizeof(string),"recv:%s",*recv);
			logIT(LOG_INFO,string);
			free(string);
			break;
		}
		alarm(CL_TIMEOUT);
	}
	if (!realloc(*recv,strlen(*recv)+1)) {
		logIT(LOG_ERR,"realloc Fehler!!");
		exit(1);
	}
	if (count <=0) {
		string=calloc(1000,sizeof(char));
		snprintf(string, sizeof(string),"exit mit count=%ld",count);
		logIT(LOG_ERR,string);;
		free(string);
	}
	return(count);
}

int connectServer(char *host) {
	char *dptr;
	int sockfd;
	char string[1000];
	if (host[0] != '/' && (dptr=strchr(host,':'))) {
                char serv[MAXBUF];
                int port;
                port=atoi(dptr+1);
                /* dptr-device liefert die Laenge des Hostes */
                bzero(serv,sizeof(host));
                strncpy(serv,host,dptr-host);
                sockfd=openCliSocket(serv,port,0);
		if (sockfd) {
			snprintf(string, sizeof(string),"Verbindung zu %s Port %d aufgebaut",host,port);
			logIT(LOG_INFO,string);
		}
		else {
			snprintf(string, sizeof(string),"Verbindung zu %s Port %d gescheitert",host,port);
			logIT(LOG_INFO,string);
			return(-1);
		}
        }
	else {
		snprintf(string, sizeof(string),"Host Format: IP|Name:Port");
		logIT(LOG_ERR,string);
		return(-1);
	}
	return(sockfd);
}

void disconnectServer(int sockfd) {
	char string[1000];
	char *ptr;
	bzero(string,sizeof(string));
	snprintf(string, sizeof(string),"quit\n");
	sendServer(sockfd,string,strlen(string));
	recvSync(sockfd,BYE,&ptr);
	free(ptr);
	close(sockfd);
}

size_t sendServer(int fd,char *s_buf, size_t len) {
	
	char string[1000];
	/* Buffer leeren */
        /* da tcflush nicht richtig funktioniert, verwenden wir nonblocking read */
        fcntl(fd,F_SETFL,O_NONBLOCK);
        while(readn(fd,string,sizeof(string))>0);
        fcntl(fd,F_SETFL,!O_NONBLOCK);
	return(Writen(fd,s_buf,len));
}

trPtr sendCmdFile(int sockfd,char *filename) {
	FILE *filePtr;
	char string[1000];
	char line[MAXBUF];
	trPtr	ptr;
	trPtr	startPtr=NULL;

	if (!(filePtr=fopen(filename,"r"))) {
		return NULL;
	}
	else {
		snprintf(string, sizeof(string),"Kommando-Datei %s geoeffnet",filename);
		logIT(LOG_INFO,string);
	}
	bzero(line,sizeof(line));
	while(fgets(line,MAXBUF-1,filePtr)){
		ptr=newTrNode(startPtr);
		if (!startPtr) {
			startPtr=ptr;
		}
		ptr->cmd=calloc(strlen(line),sizeof(char));
		strncpy(ptr->cmd,line,strlen(line)-1);
	}
	if (!sendTrList(sockfd,startPtr))  /* da ging was bei der Kommunikation schief */
		return(NULL);
	return(startPtr);
}

trPtr sendCmds(int sockfd,char *commands) {
	char *sptr;
	trPtr	ptr;
	trPtr	startPtr=NULL;

	sptr=strtok(commands,",");
	do {
		ptr=newTrNode(startPtr);
		if (!startPtr) {
			startPtr=ptr;
		}
		ptr->cmd=calloc(strlen(sptr)+1,sizeof(char));
		strncpy(ptr->cmd,sptr,strlen(sptr));
	}while((sptr=strtok(NULL,",")) != NULL);
	if (!sendTrList(sockfd,startPtr))  /* da ging was bei der Kommunikation schief */
		return(NULL);
	return(startPtr);
}

int sendTrList(int sockfd, trPtr ptr) {
	char string[1000+1];
	char prompt[]=PROMPT;
	char errTXT[]=ERR;
	char *sptr;
	char *dumPtr;

	if(recvSync(sockfd,prompt,&sptr)<=0) {
		free(sptr);
		return(0);
	}
	while(ptr){
		bzero(string,sizeof(string));
		snprintf(string, sizeof(string),"%s\n",ptr->cmd);
		if (sendServer(sockfd,string,strlen(string))<=0)
			return(0);
		bzero(string,sizeof(string));
		snprintf(string, sizeof(string),"SEND:%s",ptr->cmd);
		logIT(LOG_INFO,string);
		if(recvSync(sockfd,prompt,&sptr)<=0) {
			free(sptr);
			return(0);
		}
		ptr->raw=sptr;
		if (iscntrl(*(ptr->raw+strlen(ptr->raw)-1)))
			*(ptr->raw+strlen(ptr->raw)-1)='\0';
		dumPtr=calloc(strlen(sptr)+20,sizeof(char));
		snprintf(dumPtr,strlen(sptr)+20,"RECV:%s",sptr);
		logIT(LOG_INFO,dumPtr);
		free(dumPtr);
		/* wir fuellen Fehler und result */
		if (strstr(ptr->raw,errTXT)==ptr->raw) {
			ptr->err=ptr->raw;
			fprintf(stderr,"SRV %s\n",ptr->err);
		}
		else { /* hier suchen wir das erste Wort in raw und speichern es als result */
			char *rptr;
			char len;
			rptr=strchr(ptr->raw,' ');
			if (!rptr)
				rptr=ptr->raw+strlen(ptr->raw);
			len=rptr-ptr->raw;
			ptr->result=atof(ptr->raw);
			ptr->err=NULL;
		}
		ptr=ptr->next;
	}
	return(1);
}

static void sig_alrm(int signo) {
        longjmp(env_alrm,1);
}

