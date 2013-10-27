#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include "socket.h"


#define SERVERPORT 6578
int makeDaemon=0;
short (*checkP)(char *) = NULL;
int readCmdFile(char *filename,char *result,int *resultLen,char *device );
int interactive(int socketfd,char *device);
void printHelp(int socketfd);
int rawModus (int socketfd,char *device);
static void sigPipeHandler(int signo);
short checkIP(char *ip);

void logIT (int class,char *string) {
	printf("%s\n",string);
}

static void sigPipeHandler(int signo) {
	logIT(LOG_ERR,"SIGPIPE empfangen");
}

static void dump(char * buf, int len, char * txt)
{
	int i=0;
	printf("%s:\n", txt);
	for (i=0; i< len; i++) {
		printf(" %02x", (unsigned char)buf[i]);
	}
	printf("\n");
}

typedef struct  {
	int cmdlen;
	char cmd[20];
	int rsplen;
	char rsp[20];
} ctable;

int cmdc = 7;
ctable cmds[] = {
		{ 1, { 0x04 }, 1, { 0x05 } },
		{ 3, { 0x16, 0x00, 0x00 }, 1, { 0x06 } },
		{ 5, { 0x01, 0xf7, 0xcb, 0x70, 01 }, 2, {0x30, 0x00 } },
		{ 8, { 0x41, 0x05, 0x00, 0x01, 0x55, 0x25, 0x02, 0x82 },
				11, { 0x06, 0x41, 0x07, 0x01, 0x01, 0x55, 0x25, 0x02, 0x07, 0x01, 0x8D } },
    	{ 8, { 0x41, 0x05, 0x00, 0x01, 0x08, 0x00, 0x02, 0x10 },
				11, { 0x06, 0x41, 0x07, 0x01, 0x01, 0x08, 0x00, 0x02, 0x07, 0x01, 0x1B } },
		{ 8, { 0x41, 0x05, 0x00, 0x01, 0x21, 0x10, 0x08, 0x3F },
				16, { 0x06, 0x41, 0x0D, 0x01, 0x01, 0x21, 0x10, 0x08, 0x28, 0xB0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } },
		{ 5, { 0x01, 0xf7, 0x08, 0x00, 0x02 },
				 2, { 0x07, 0x01 },
		}
};

char input[100];
char inpidx=0;

static void handle(int fd)
{
	char buf[1] = "\0";
	ssize_t len;

	while ( 1 ) {
		len = readn(fd, buf, 1);
		if  (len < 0) {
			perror("read eror\n");
			exit(-1);
		} else if (len == 0) {
			printf("eof read\n");
			return;
		} else {
			int i = 0;
			int j = 0;
			while ( i < len ) {
				input[inpidx] = buf[i];
				dump(&input[inpidx],1,"received char:");
				inpidx++;
				for ( j = 0 ; j < cmdc; j++) {
					if ( cmds[j].cmd[0] == input[0] ) {
						if (( cmds[j].cmdlen == inpidx) && (!memcmp(cmds[j].cmd,input,inpidx))) {
							dump(input,inpidx,"received cmd:");
							dump(cmds[j].rsp, cmds[j].rsplen,"answer:");
							if (writen(fd, cmds[j].rsp, cmds[j].rsplen) != cmds[j].rsplen) {
								printf("not completely written\n");
							}
							inpidx=0;
						}
					}
				}
				i++;

			}
		}
	}
}

int main(int argc, char *argv[])
{
	int sockfd=-1;
	int listenfd=-1;

	listenfd = openSocket(SERVERPORT);
	while(1) {
		sockfd=listenToSocket(listenfd,makeDaemon,checkP);
		if (signal(SIGPIPE,sigPipeHandler)== SIG_ERR) {
			logIT(LOG_ERR,"Signal error");
			exit(1);
		}
		if (sockfd>=0) {
			/* Socket hat fd zurueckgegeben, den Rest machen wir in interactive */
			logIT(LOG_INFO,"\nvcontrold connected");
			handle(sockfd);
			logIT(LOG_INFO,"\nvcontrold disconnected");
			closeSocket(sockfd);
		}
		else {
			logIT(LOG_ERR,"Fehler bei Verbindungsaufbau");
		}
	}
}
