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
		VCLog(LOG_EMERG, "cannot open %s:%s", device, strerror (errno));
		exit(1);
	}
	int s;
	struct termios oldsb, newsb;
	s=tcgetattr(fd,&oldsb);

	if (s<0) {
		VCLog(LOG_EMERG, "error tcgetattr %s:%s", device, strerror (errno));
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
newsb.c_lflag = 0;
	newsb.c_cflag = (CLOCAL | B4800 | CS8 | CREAD| PARENB | CSTOPB );
	newsb.c_cc[VMIN]   = 1;
	newsb.c_cc[VTIME]  = 0;

	/* tcsetattr(fd, TCSADRAIN, &newsb); */
	if (tcflush(fd, TCIFLUSH) < 0)
	  VCLog(LOG_ERR, "Error in tcflush %d:%s", errno, strerror (errno));
	if (tcsetattr(fd, TCSANOW, &newsb) < 0)
	  VCLog(LOG_ERR, "Error in tcsetattr %d:%s", errno, strerror (errno));

	/* DTR High fuer Spannungsversorgung */
	/*
	int modemctl = 0;
	ioctl(fd, TIOCMGET, &modemctl);
	modemctl |= TIOCM_DTR;
	modemctl &= !TIOCM_DTR;
	s=ioctl(fd,TIOCMSET,&modemctl);
	if (s<0) {
		VCLog(LOG_EMERG, "error ioctl TIOCMSET %s:%s", device, strerror (errno));
		exit(1); 
	}
	*/
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

int receive(int fd,char *r_buf,int r_len,unsigned long *etime)
{
  int i;
  int rv;
  fd_set set;
  struct timeval timeout;
  struct tms tms_t;
  clock_t start,mid,mid1;
  unsigned long clktck;
  unsigned long totaltime;
  clktck=sysconf(_SC_CLK_TCK);
  totaltime = 0;
  timeout.tv_sec = TIMEOUT;
  timeout.tv_usec = 0;

  FD_ZERO(&set); /* clear the set */
  FD_SET(fd, &set); /* add our file descriptor to the set */

  start=times(&tms_t);
  mid1=start;
  for (i=0; i<r_len; i++) {
    if (totaltime > (1000*TIMEOUT)) {
      VCLog(LOG_ERR, "Cumulative timeout in read: read took %ld msec", totaltime);
      return(-1);
    }
    rv = select(fd + 1, &set, NULL, NULL, &timeout);
    if(rv == -1) {
      VCLog(LOG_ERR, "Error in select call %d(%s)", errno, strerror(errno));
      return(-1);
    }
    else 
      if(rv == 0) {
	VCLog(LOG_ERR, "Individual timeout in read");
	return(-1);
      }
      else {
	if (read(fd,&r_buf[i],1) <= 0) { /* there was data to read */
	  VCLog(LOG_ERR, "Error read tty %d(%s)", errno, strerror(errno));
	  return(-1);      
	}
      }
    unsigned char byte=r_buf[i] & 255;
    mid=times(&tms_t);
    VCLog(LOG_INFO,"<RECV: %02X (%0.1f ms)", byte,((float)(mid-mid1)/clktck)*1000);
    mid1=mid;
    totaltime=((float)(mid-start)/clktck)*1000;
  }
  *etime = totaltime;
  return i;
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
			VCLog(LOG_EMERG, "Synchronisation verloren");
			exit(1);
		}
        }		
	return(1);
}
