/* io.h 
 *
*/
/* $Id: io.h 34 2008-04-06 19:39:29Z marcust $ */

int my_send(int fd,char *s_buf,int len);
int receive(int fd,char *r_buf,int r_len,unsigned long *etime);
int waitfor(int fd, char *w_buf, int w_len);
int opentty(char *device);
int openDevice(char *device);


/* Schnittstellenparameter */

#define TTY    "/dev/usb/tts/0"

#ifndef MAXBUF
	#define MAXBUF 4096
#endif
#define TIMEOUT 5 


