/* common.h */
/* $Id: common.h 26 2008-03-20 20:56:09Z marcust $ */

/* Deklarationen */
int initLog(int useSyslog, char *logfile,int debugSwitch);
void logIT (int class,char *string);
char hex2chr(char *hex);
int char2hex(char *outString, const char *charPtr, int len);
short string2chr(char *line,char *buf,short bufsize);
void sendErrMsg(int fd);
void setDebugFD(int fd);
ssize_t readn(int fd, void *vptr, size_t n);

#ifndef MAXBUF
        #define MAXBUF 4096 
#endif


