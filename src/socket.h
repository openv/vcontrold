// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SOCKET_H
#define SOCKET_H

#include <arpa/inet.h>

int openSocket(int tcpport);
int openSocket2(int tcpport, const char* aname);
int listenToSocket(int listenfd, int makeChild);
int openCliSocket(char *host, int port, int noTCPdelay);
void closeSocket(int sockfd);

ssize_t writen(int fd, const void *vptr, size_t n);
ssize_t Writen(int fd, void *ptr, size_t nbytes);

ssize_t readn(int fd, void *vptr, size_t n);
ssize_t Readn(int fd, void *ptr, size_t nbytes);

ssize_t readline(int fd, void *vptr, size_t maxlen);
ssize_t Readline(int fd, void *ptr, size_t maxlen);

#define LISTENQ 1024
#define MAXLINE 1000

#define CONNECT_TIMEOUT 3

#endif // SOCKET_H
