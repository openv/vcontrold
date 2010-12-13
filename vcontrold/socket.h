/* socket.h */
/* $Id: socket.h 13 2008-03-02 13:13:41Z marcust $ */

#include<arpa/inet.h>

int openSocket(int tcpport);
int listenToSocket(int listenfd,int makeChild,short (*checkP)(char *));
int openCliSocket(char *host,int port, int noTCPdelay);
void closeSocket(int sockfd);

#define LISTENQ 1024
#define MAXLINE 1000

#define CONNECT_TIMEOUT 3 
