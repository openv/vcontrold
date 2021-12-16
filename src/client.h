// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CLIENT_H
#define CLIENT_H

#define CL_TIMEOUT 25

#ifndef MAXBUF
#define MAXBUF 4096
#endif

#define ALLOCSIZE 256

typedef struct txRx *trPtr;

ssize_t recvSync(int fd, char *wait, char **recv);
int connectServer(char *host, int port);
void disconnectServer(int sockfd);
size_t sendServer(int fd, char *s_buf, size_t len);
trPtr sendCmdFile(int sockfd, const char *tmpfile);
trPtr sendCmds(int sockfd, char *commands);

typedef struct txRx {
    char *cmd;
    float result;
    char *err;
    char *raw;
    time_t timestamp;
    trPtr next;
} TxRx;

#endif // CLIENT_H
