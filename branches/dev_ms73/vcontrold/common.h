/* common.h */
/* $Id: common.h 26 2008-03-20 20:56:09Z marcust $ */

#ifndef _COMMON_H_
#define _COMMON_H_

/* Deklarationen */

#define LOG_BUFFER_SIZE 1024
#define VCLog(LEVEL,...) do { char buf[LOG_BUFFER_SIZE]; snprintf(buf, LOG_BUFFER_SIZE-2, __VA_ARGS__ ); logWrite(LEVEL, buf, __FILE__, __LINE__); } while (0)

int initLog(int useSyslog, char *logfile,int debugSwitch);
void logWrite (int class,char *string, char* sourcefile, long sourceline);
void XlogIT (int class,char *string);
char hex2chr(char *hex);
int char2hex(char *outString, const char *charPtr, int len);
short string2chr(char *line,char *buf,short bufsize);
void sendErrMsg(int fd);
void setDebugFD(int fd);
ssize_t readn(int fd, void *vptr, size_t n);

#define MAXBUF 4096 

#endif // _COMMON_H_
