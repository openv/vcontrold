/* socket.c */
/* $Id: socket.c 13 2008-03-02 13:13:41Z marcust $ */

#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifndef __CYGWIN__
/* I'm not sure about this cpp defines, can some check tht? -fn- */
#ifdef __linux__
#include <linux/tcp.h>	/*do we realy need this? Not sure for Linux -fn- */
#endif
#if defined (__FreeBSD__) || defined(__APPLE__)
#include <netinet/in.h>
#include <netinet/tcp.h>	/* TCP_NODELAY is defined there -fn- */
#endif
#endif

#include "socket.h"
#include "common.h"


static void sig_alrm(int);
static jmp_buf  env_alrm;



int openSocket(int tcpport) {
	int listenfd;
	struct sockaddr_in servaddr;

	listenfd=socket(AF_INET,SOCK_STREAM,0);
	
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family =AF_INET;
	servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	servaddr.sin_port=htons(tcpport);
	
	// this will configure the socket to reuse the address if it was in use and is not free allready.
	int optval =1;
	if(listenfd < 0 || setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof optval) <0 ) {
		VCLog(LOG_EMERG, "setsockopt gescheitert!");
		exit(1);
	}
	
	if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr))) {
			VCLog(LOG_EMERG, "bind auf port %d gescheitert (in use / closewait)", tcpport);
			exit(1);
	}
	VCLog(LOG_NOTICE, "TCP socket %d geoeffnet", tcpport);
	listen(listenfd, LISTENQ);
	return(listenfd);
}

int listenToSocket(int listenfd,int makeChild,short (*checkP)(char *)) {
	int connfd;
	pid_t	childpid;
	socklen_t clilen;
	struct sockaddr_in cliaddr;
	char buf[1000];
	int cliport;

	signal(SIGCHLD,SIG_IGN);

	for( ;;) {
		clilen=sizeof(cliaddr);
		connfd=accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);	
		inet_ntop(AF_INET,&cliaddr.sin_addr, buf, sizeof(buf));
		cliport=ntohs(cliaddr.sin_port);
		if(checkP && !(*checkP)(buf)) {
			VCLog(LOG_NOTICE, "Access denied %s:%d", buf, cliport);
			close(connfd);
			continue;
		}
		VCLog(LOG_NOTICE, "Client verbunden %s:%d (FD:%d)", buf, cliport, connfd);
		if (!makeChild) {
			return(connfd);
		}
		else if ( (childpid=fork())==0) { /* unser Kind */
			close(listenfd);
			return(connfd);
		}
		else {
			VCLog(LOG_INFO, "Child Prozess mit pid:%d gestartet", childpid);
		}
		close(connfd);
	}
}

void closeSocket(int sockfd)
{
        VCLog(LOG_INFO, "Verbindung beendet (fd:%d)", sockfd);
	close(sockfd);
}

int openCliSocket(char *host,int port, int noTCPdelay) {
	struct hostent *hp;
	struct in_addr **pptr;
	struct sockaddr_in servaddr;
	int sockfd;
	extern int errno;
	char *errstr;

	if ((hp=gethostbyname(host))== NULL) {	
		VCLog(LOG_EMERG, "Fehler gethostbyname: %s:%s", host, hstrerror(h_errno));
		exit(1);
	}
	pptr= (struct in_addr **) hp->h_addr_list;
	for( ; *pptr != NULL; pptr++) {
		sockfd=socket(AF_INET,SOCK_STREAM,0);
		bzero(&servaddr,sizeof(servaddr));
		servaddr.sin_family=AF_INET;
		servaddr.sin_port=htons(port);
		memcpy(&servaddr.sin_addr,*pptr,sizeof(struct in_addr));
		if (signal(SIGALRM, sig_alrm) == SIG_ERR)
			VCLog(LOG_ERR,"SIGALRM error");
		if(setjmp(env_alrm) !=0) {
			VCLog(LOG_ERR, "connect timeout %s:%d", host, port);
			close(sockfd); /* anscheinend besteht manchmal schon noch eine Verbindung??? */
			alarm(0);
                        return(-1);
                }
                alarm(CONNECT_TIMEOUT);
		if(connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == 0) {
			alarm(0);
			break; /* wir haben eine Verbindung */
		}
		alarm(0);
		close(sockfd);
	}
	if (*pptr == NULL) {
		VCLog(LOG_ERR, "TTY Net: Keine Verbingung zu %s:%d", host, port);
		return(-1);
	}
	VCLog(LOG_INFO, "ClI Net: verbunden %s:%d (FD:%d)", host, port, sockfd);
	int flag=1;
	if (noTCPdelay && (setsockopt(sockfd,IPPROTO_TCP,TCP_NODELAY, (char*) &flag,sizeof(int)))) {
		errstr=strerror(errno);
		VCLog(LOG_ERR, "Fehler setsockopt TCP_NODELAY (%s)", errstr);
	}
	

	return(sockfd);
}

static void sig_alrm(int signo) {
	longjmp(env_alrm,1);
}
		
		
		
	

/* Stuff aus Unix Network Programming Vol 1*/

/* include writen */

ssize_t						/* Write "n" bytes to a descriptor. */
writen(int fd, const void *vptr, size_t n)
{
	size_t		nleft;
	ssize_t		nwritten;
	const char	*ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
			if (errno == EINTR)
				nwritten = 0;		/* and call write() again */
			else
				return(-1);			/* error */
		}

		nleft -= nwritten;
		ptr   += nwritten;
	}
	return(n);
}
/* end writen */

ssize_t 
Writen(int fd, void *ptr, size_t nbytes)
{
	if (writen(fd, ptr, nbytes) != nbytes) {
		VCLog(LOG_ERR, "Fehler beim schreiben auf socket");
		return(0);
	}
	return(nbytes);
}


/* include readn */

ssize_t						/* Read "n" bytes from a descriptor. */
readn(int fd, void *vptr, size_t n)
{
	size_t	nleft;
	ssize_t	nread;
	char	*ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nread = read(fd, ptr, nleft)) < 0) {
			if (errno == EINTR)
				nread = 0;		/* and call read() again */
			else
				return(-1);
		} else if (nread == 0)
			break;				/* EOF */

		#ifdef __CYGWIN__
		if(nread > nleft) 				// This is a workaround for Cygwin.    
			nleft=0;					// Here cygwins read(fd,buff,count) is
		else							// reading more than count chars! this is bad!
			nleft -= nread;
		#else
		nleft -= nread;
		#endif
		ptr   += nread;
	}
	return(n - nleft);		/* return >= 0 */
}
/* end readn */

ssize_t
Readn(int fd, void *ptr, size_t nbytes)
{
	ssize_t		n;

	if ( (n = readn(fd, ptr, nbytes)) < 0) {
		VCLog(LOG_ERR,"Fehler beim lesen von socket");
		return(0);
	}
	return(n);
}


/* include readline */

static ssize_t
my_read(int fd, char *ptr)
{
	static ssize_t read_cnt = 0;
	static char	*read_ptr;
	static char	read_buf[MAXLINE];

	if (read_cnt <= 0) {
again:
		if ( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {
			if (errno == EINTR)
				goto again;
			return(-1);
		} else if (read_cnt == 0)
			return(0);
		read_ptr = read_buf;
	}

	read_cnt--;
	*ptr = *read_ptr++;
	return(1);
}

ssize_t
readline(int fd, void *vptr, size_t maxlen)
{
	int		n;
	ssize_t rc;
	char	c, *ptr;

	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
		if ( (rc = my_read(fd, &c)) == 1) {
			*ptr++ = c;
			if (c == '\n')
				break;	/* newline is stored, like fgets() */
		} else if (rc == 0) {
			if (n == 1)
				return(0);	/* EOF, no data read */
			else
				break;		/* EOF, some data was read */
		} else
			return(-1);		/* error, errno set by read() */
	}

	*ptr = 0;	/* null terminate like fgets() */
	return(n);
}
/* end readline */

ssize_t
Readline(int fd, void *ptr, size_t maxlen)
{
	ssize_t		n;

	if ( (n = readline(fd, ptr, maxlen)) < 0) {
		VCLog(LOG_ERR, "Fehler beim lesen von socket");
		return(0);
	}
	return(n);
}
