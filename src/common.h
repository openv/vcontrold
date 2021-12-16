// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COMMON_H
#define COMMON_H

int initLog(int useSyslog, char *logfile, int debugSwitch);
void logIT (int class, char *string, ...);
char hex2chr(char *hex);
int char2hex(char *outString, const char *charPtr, int len);
short string2chr(char *line, char *buf, short bufsize);
void sendErrMsg(int fd);
void setDebugFD(int fd);
ssize_t readn(int fd, void *vptr, size_t n);

#ifndef MAXBUF
#define MAXBUF 4096
#endif

#ifndef DEFAULT_PORT
#define DEFAULT_PORT 3002
#endif

#define logIT1(class, string) logIT(class, "%s", string)

#endif // COMMON_H
