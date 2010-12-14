#ifndef _COMMON_H_
#define _COMMON_H_

/* common.h */
/* $Id: common.h 26 2008-03-20 20:56:09Z marcust $ */

#include <syslog.h> 

/* Deklarationen */
int initLog(int useSyslog, char *logfile,int debugSwitch);
void logIT (int class,char *string);
char hex2chr(char *hex);
int char2hex(char *outString, const char *charPtr, int len);
short string2chr(char *line,char *buf,short bufsize);
void sendErrMsg(int fd);
void setDebugFD(int fd);

#ifndef MAXBUF
        #define MAXBUF 4096 
#endif

#endif /* _COMMON_H_ */
