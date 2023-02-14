// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PARSER_H
#define PARSER_H

int parseLine(char *lineo, char *hex, int *hexlen, char *uSPtr, ssize_t uSPtrLen);
int execCmd(char *cmd, int fd, char *result, int resultLen);
void removeCompileList(compilePtr ptr);
int execByteCode(compilePtr cmpPtr, int fd, char *recvBuf, short recvLen, char *sendBuf,
                 short sendLen, short supressUnit, char bitpos, int retry, char *pRecvPtr,
                 unsigned short recvTimeout);
void compileCommandAll(devicePtr dPtr, unitPtr uPtr);

// Token Definition
#define WAIT    1
#define RECV    2
#define SEND    3
#define PAUSE   4
#define BYTES   5

#ifndef MAXBUF
#define MAXBUF 4096
#endif

#endif // PARSER_H
