/* socket.h */
/* $Id: socket.h 13 2008-03-02 13:13:41Z marcust $ */

#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <arpa/inet.h>

int openSocket(int tcpport);
int listenToSocket(int listenfd,int makeChild,short (*checkP)(char *));
int openCliSocket(char *host,int port, int noTCPdelay);
void closeSocket(int sockfd);

ssize_t	writen(int fd, const void *vptr, size_t n);
ssize_t Writen(int fd, void *ptr, size_t nbytes);

ssize_t	readn(int fd, void *vptr, size_t n);
ssize_t Readn(int fd, void *ptr, size_t nbytes);

ssize_t readline(int fd, void *vptr, size_t maxlen);
ssize_t Readline(int fd, void *ptr, size_t maxlen);

#define LISTENQ 1024
#define MAXLINE 1000

#define CONNECT_TIMEOUT 3 

#endif // _SOCKET_H_
