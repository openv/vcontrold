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

#define _GNU_SOURCE

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
#include <netinet/tcp.h> // TCP_NODELAY is defined there -fn-
#if defined (__FreeBSD__) || defined(__APPLE__)
#include <netinet/in.h>
#endif

#include "socket.h"
#include "common.h"
#include "vclient.h"

const int LISTEN_QUEUE = 128;

int openSocket(int tcpport)
{
    int listenfd;
    int n;
    char *port;
    struct addrinfo hints, *res, *ressave;

    memset(&hints, 0, sizeof(struct addrinfo));

    switch (inetversion) {
    case 6:
        hints.ai_family = PF_INET6;
        break;
    case 4:
        // this is for backward compatibility. We can explictly
        // activate IPv6 with the -6 switch. Later we can use
        // PF_UNSPEC as default and let the OS decide
    default:
        hints.ai_family = PF_INET;
    }

    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags    = AI_PASSIVE;

    asprintf(&port, "%d", tcpport);

    n = getaddrinfo(NULL, port, &hints, &res);

    free(port);

    if (n < 0) {
        logIT(LOG_ERR, "getaddrinfo error: [%s]\n", gai_strerror(n));
        return -1;
    }

    ressave = res;

    // Try open socket with each address getaddrinfo returned,
    // until getting a valid listening socket.

    listenfd = -1;
    while (res) {
        listenfd = socket(res->ai_family,
                          res->ai_socktype,
                          res->ai_protocol);
        int optval = 1;
        if (listenfd > 0 && setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                                       &optval, sizeof optval) < 0 ) {
            logIT1(LOG_ERR, "setsockopt failed!");
            exit(1);
        }
        if (! (listenfd < 0)) {
            if (bind(listenfd, res->ai_addr, res->ai_addrlen) == 0) {
                break;
            }
            close(listenfd);
            listenfd = -1;
        }
        res = res->ai_next;
    }

    if (listenfd < 0) {
        freeaddrinfo(ressave);
        fprintf(stderr, "socket error: could not open socket\n");
        exit(1);
    }

    listen(listenfd, LISTEN_QUEUE);
    logIT(LOG_NOTICE, "TCP socket %d opened", tcpport);

    freeaddrinfo(ressave);

    return listenfd;
}

int listenToSocket(int listenfd, int makeChild, short (*checkP)(char *))
{
    int connfd;
    pid_t childpid;
    struct sockaddr_storage cliaddr;
    socklen_t cliaddrlen = sizeof(cliaddr);
    char clienthost   [NI_MAXHOST];
    char clientservice[NI_MAXSERV];

    signal(SIGCHLD, SIG_IGN);

    for (;;) {
        connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &cliaddrlen);
        getnameinfo((struct sockaddr *) &cliaddr, cliaddrlen, clienthost, sizeof(clienthost),
                    clientservice, sizeof(clientservice), NI_NUMERICHOST);

        if (connfd < 0) {
            logIT(LOG_NOTICE, "accept on host %s: port %s", clienthost, clientservice);
            close(connfd);
            continue;
        }

        logIT(LOG_NOTICE, "Client connected %s:%s (FD:%d)", clienthost, clientservice, connfd);
        if (! makeChild) {
            return connfd;
        } else if ( (childpid = fork()) == 0) {
            // unser Kind
            close(listenfd);
            return connfd;
        } else {
            logIT(LOG_INFO, "Child process started with pid %d", childpid);
        }

        close(connfd);
    }
}

void closeSocket(int sockfd)
{
    logIT(LOG_INFO, "Closed connection (fd:%d)", sockfd);
    close(sockfd);
}

int openCliSocket(char *host, int port, int noTCPdelay)
{
    struct addrinfo hints; // use hints for ipv46 address resolution
    struct addrinfo *res;
    struct addrinfo *ressave;
    int n;
    int sockfd;
    char port_string[16]; // the IPv6 world use a char* instead of an int in getaddrinfo

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family   = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_ALL | AI_V4MAPPED;

    snprintf(port_string, sizeof(port_string), "%d", port);
    n = getaddrinfo(host, port_string, &hints, &res);
    if (n < 0) {
        logIT(LOG_ERR, "Error in getaddrinfo: %s:%s", host, gai_strerror(n));
        exit(1);
    }

    ressave = res;

    sockfd = -1;
    while (res) {
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (! (sockfd < 0)) {
            if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0) {
                break;
            }
            // we have a succesfull connection
            close(sockfd);
            sockfd = -1;
        }
        res = res->ai_next;
    }

    freeaddrinfo(ressave);

    if (sockfd < 0) {
        logIT(LOG_ERR, "TTY Net: No connection to %s:%d", host, port);
        return -1;
    }
    logIT(LOG_INFO, "ClI Net: connected %s:%d (FD:%d)", host, port, sockfd);
    int flag = 1;
    if (noTCPdelay && (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int)))) {
        logIT(LOG_ERR, "Error in setsockopt TCP_NODELAY (%s)", strerror(errno));
    }

    return sockfd;
}

// Stuff aus Unix Network Programming Vol 1
// include writen

// Write "n" bytes to a descriptor.
ssize_t writen(int fd, const void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if (errno == EINTR) {
                // and call write() again
                nwritten = 0;
            }  else {
                // error
                return -1;
            }
        }
        nleft -= nwritten;
        ptr   += nwritten;
    }
    return n;
}

// end writen

ssize_t Writen(int fd, void *ptr, size_t nbytes)
{
    if (writen(fd, ptr, nbytes) != nbytes) {
        logIT1(LOG_ERR, "Error writing to socket");
        return 0;
    }
    return nbytes;
}

// include readn

// Read "n" bytes from a descriptor.
ssize_t readn(int fd, void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nread;
    char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR) {
                // and call read() again
                nread = 0;
            }
            else {
                return -1;
            }
        } else if (nread == 0) {
            // EOF
            break;
        }

#ifdef __CYGWIN__
        // This is a workaround for Cygwin.
        // Here cygwins read(fd,buff,count) is reading more than count chars! this is bad!
        if (nread > nleft) {
            nleft = 0;
        } else {
            nleft -= nread;
        }
#else
        nleft -= nread;
#endif
        ptr   += nread;
    }

    return n - nleft; //return >= 0
}

// end readn

ssize_t Readn(int fd, void *ptr, size_t nbytes)
{
    ssize_t n;
    if ((n = readn(fd, ptr, nbytes)) < 0) {
        logIT1(LOG_ERR, "Error reading from socket");
        return 0;
    }
    return n;
}

// include readline

static ssize_t my_read(int fd, char *ptr)
{

    static ssize_t read_cnt = 0;
    static char *read_ptr;
    static char read_buf[MAXLINE];

    if (read_cnt <= 0) {
again:
        if ((read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {
            if (errno == EINTR) {
                goto again;
            }
            return -1;
        } else if (read_cnt == 0) {
            return 0;
        }
        read_ptr = read_buf;
    }

    read_cnt--;
    *ptr = *read_ptr++;
    return 1;
}

ssize_t readline(int fd, void *vptr, size_t maxlen)
{
    int n;
    ssize_t rc;
    char c;
    char *ptr;

    ptr = vptr;
    for (n = 1; n < maxlen; n++) {
        if ( (rc = my_read(fd, &c)) == 1) {
            *ptr++ = c;
            if (c == '\n') {
                // newline is stored, like fgets()
                break;
            }
        } else if (rc == 0) {
            if (n == 1) {
                // EOF, no data read
                return 0;
            }
            else {
                // EOF, some data was read
                break;
            }
        } else {
            // error, errno set by read()
            return -1;
        }
    }

    *ptr = 0; // null terminate like fgets()
    return n;
}

// end readline

ssize_t Readline(int fd, void *ptr, size_t maxlen)
{
    ssize_t n;

    if ((n = readline(fd, ptr, maxlen)) < 0) {
        logIT1(LOG_ERR, "Error reading from socket");
        return 0;
    }
    return n;
}
