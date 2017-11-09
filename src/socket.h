/*  Copyright 2007-2017 the original vcontrold development team

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include<arpa/inet.h>

int openSocket(int tcpport);
int listenToSocket(int listenfd, int makeChild, short (*checkP)(char *));
int openCliSocket(char *host, int port, int noTCPdelay);
void closeSocket(int sockfd);

ssize_t    writen(int fd, const void *vptr, size_t n);
ssize_t Writen(int fd, void *ptr, size_t nbytes);

ssize_t    readn(int fd, void *vptr, size_t n);
ssize_t Readn(int fd, void *ptr, size_t nbytes);

ssize_t readline(int fd, void *vptr, size_t maxlen);
ssize_t Readline(int fd, void *ptr, size_t maxlen);

#define LISTENQ 1024
#define MAXLINE 1000

#define CONNECT_TIMEOUT 3
