/*
 * io.c Kommunikation mit der Vito* Steuerung 
 *  */
/* $Id: io.c 34 2008-04-06 19:39:29Z marcust $ */
#include <stdlib.h>
#include <stdio.h> 
#include <errno.h>
#include <signal.h>  
#include <syslog.h> 
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <time.h>
#include <setjmp.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/ioctl.h>

#include "io.h"
#include "socket.h"
#include "common.h"

#ifdef __CYGWIN__
/* NCC is not defined under cygwin */
#define NCC NCCS
#endif

static void sig_alrm(int);
static jmp_buf	env_alrm;

int openDevice(char *device) {
	/* wir unterscheiden hier TTY und Socket Verbindung */
	/* Socket: kein / am Anfang und ein : */
	int fd;
	char *dptr;

	if (device[0] != '/' && (dptr=strchr(device,':'))) {
		char host[MAXBUF];
		int port;
		port=atoi(dptr+1);
		/* dptr-device liefert die Laenge des Hostes */
		bzero(host,sizeof(host));
		strncpy(host,device,dptr-device);
		/* 3. Parameter ==1 --->noTCPDelay wird gesetzt */
		fd=openCliSocket(host,port,1);
	}	
	else 
		fd=opentty(device);
	if (fd <0 ) {
		/* hier kann noch Fehlerkram rein */
		return(-1);
	}
	return(fd);
}


int opentty(char *device) {
	int fd;
	VCLog(LOG_INFO, "konfiguriere serielle Schnittstelle %s", device);
	if ((fd=open(device,O_RDWR)) < 0) {
		VCLog(LOG_ERR, "cannot open %s:%s", device, strerror (errno));
		exit(1);
	}
	int s;
	struct termios oldsb, newsb;
	s=tcgetattr(fd,&oldsb);

	if (s<0) {
		VCLog(LOG_ERR, "error tcgetattr %s:%s", device, strerror (errno));
		exit(1);
	}
	newsb=oldsb;

#ifdef NCC
	/* NCC is not always defined */
	for (s = 0; s < NCC; s++)
		newsb.c_cc[s] = 0;
#else
	bzero (&newsb, sizeof(newsb));
#endif
	newsb.c_iflag=IGNBRK | IGNPAR;
	newsb.c_oflag = 0;
	newsb.c_lflag = ISIG;
	newsb.c_cflag = (CLOCAL | B4800 | CS8 | CREAD| PARENB | CSTOPB);
	newsb.c_cc[VMIN]   = 1;
	newsb.c_cc[VTIME]  = 0;

	tcsetattr(fd, TCSADRAIN, &newsb);

	/* DTR High fuer Spannungsversorgung */
	int modemctl = 0;
	ioctl(fd, TIOCMGET, &modemctl);
	modemctl |= TIOCM_DTR;
	s=ioctl(fd,TIOCMSET,&modemctl);
	if (s<0) {
		VCLog(LOG_ERR, "error ioctl TIOCMSET %s:%s", device, strerror (errno));
		exit(1); 
	}

	return(fd);
}

int my_send(int fd,char *s_buf, int len) {
	int i;
	char string[1000];

	/* Buffer leeren */
	/* da tcflush nicht richtig funktioniert, verwenden wir nonblocking read */
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	while(readn(fd,string,sizeof(string))>0);
	fcntl(fd, F_SETFL, flags);
	
	tcflush(fd,TCIFLUSH);
	for (i=0;i<len;i++) {
		/* wir benutzen die Socket feste Vairante aus socket.c */
		writen(fd,&s_buf[i],1);
		unsigned char byte=s_buf[i] & 255;
		VCLog(LOG_INFO, ">SEND: %02X", (int)byte);
	}
	return(1);
}

int receive(int fd,char *r_buf,int r_len,unsigned long *etime) {
	int i;

	struct tms tms_t;
	clock_t start,end,mid,mid1;
	unsigned long clktck;
	clktck=sysconf(_SC_CLK_TCK);
	start=times(&tms_t);
	mid1=start;
	for(i=0;i<r_len;i++) {
	  if (signal(SIGALRM, sig_alrm) == SIG_ERR)
	    VCLog(LOG_ERR, "SIGALRM error");
	  if(setjmp(env_alrm) !=0) {
	    VCLog(LOG_ERR, "read timeout");
	    return(-1);
	  }
	  alarm(TIMEOUT);
	  /* wir benutzen die Socket feste Vairante aus socket.c */
	  if (readn(fd,&r_buf[i],1) <= 0) {
	    VCLog(LOG_ERR, "error read tty");;
	    alarm(0);
	    return(-1);
	  }
	  alarm(0);
	  unsigned char byte=r_buf[i] & 255;
	  mid=times(&tms_t);
	  VCLog(LOG_INFO,"<RECV: %02X (%0.1f ms)", byte,((float)(mid-mid1)/clktck)*1000);
	  mid1=mid;
	}
	end=times(&tms_t);
	*etime=((float)(end-start)/clktck)*1000;
	return i;
}

static void sig_alrm(int signo) {
	longjmp(env_alrm,1);
}

int waitfor(int fd, char *w_buf,int w_len) {
	int i;
	time_t start;
	char r_buf[MAXBUF];
	char hexString[1000]="\0";
	char dummy[3];
	unsigned long etime;
	for(i=0;i<w_len;i++) {
		sprintf(dummy,"%02X",w_buf[i]);	
		strncat(hexString,dummy,999);
	}
	
	VCLog(LOG_INFO, "Warte auf %s", hexString);
	start=time(NULL);
	
	/* wir warten auf das erste Zeichen, danach muss es passen */
	do {
		etime=0;
		if (receive(fd,r_buf,1,&etime)<0) 
			return(0);
		if (time(NULL)-start > TIMEOUT) {
			VCLog(LOG_WARNING, "Timeout wait");
			return(0);
		}
	} while (r_buf[0] != w_buf[0]);
	for(i=1;i<w_len;i++) {
		etime=0;
		if (receive(fd,r_buf,1,&etime)<0) 
			return(0);
		if (time(NULL)-start > TIMEOUT) {
			VCLog(LOG_WARNING, "Timeout wait");
			return(0);
		}
		if( r_buf[0] != w_buf[i]) {
			VCLog(LOG_ERR, "Synchronisation verloren");
			exit(1);
		}
        }		
	return(1);
}
