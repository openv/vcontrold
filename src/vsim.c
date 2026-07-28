// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

#define _GNU_SOURCE

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
#include <stdarg.h>
#include "socket.h"
#include "vclient.h"

#define SERVERPORT 6578
int makeDaemon = 0;
int inetversion = 0;
int readCmdFile(char *filename, char *result, int *resultLen, char *device );
int interactive(int socketfd, char *device);
void printHelp(int socketfd);
int rawModus (int socketfd, char *device);
static void sigPipeHandler(int signo);

void logIT (int class, char *string, ...)
{
    va_list arguments;
    char *print_buffer = NULL;

    (void)class;
    va_start(arguments, string);
    if (vasprintf(&print_buffer, string, arguments) < 0) {
        print_buffer = NULL;
    }
    va_end(arguments);
    if (!print_buffer) return;
    printf("%s\n", print_buffer);
    free(print_buffer);
}

static void sigPipeHandler(int signo)
{
    (void)signo;
    logIT(LOG_ERR, "Received SIGPIPE");
}

static void dump(const unsigned char *buf, size_t len, const char *txt)
{
    size_t i = 0;
    printf("%s:\n", txt);
    for (i = 0; i < len; i++) {
        printf(" %02x", (unsigned char)buf[i]);
    }
    printf("\n");
}

typedef struct {
    size_t cmdlen;
    unsigned char cmd[20];
    size_t rsplen;
    unsigned char rsp[20];
} ctable;

int cmdc = 9;
ctable cmds[] = {
    { 1, { 0x04 }, 1, { 0x05 } },
    { 3, { 0x16, 0x00, 0x00 }, 1, { 0x06 } },
    { 5, { 0x01, 0xf7, 0xcb, 0x70, 01 }, 2, {0x30, 0x00 } },
    {
        8, { 0x41, 0x05, 0x00, 0x01, 0x55, 0x25, 0x02, 0x82 },
        11, { 0x06, 0x41, 0x07, 0x01, 0x01, 0x55, 0x25, 0x02, 0x07, 0x01, 0x8D }
    },
    {
        8, { 0x41, 0x05, 0x00, 0x01, 0x08, 0x00, 0x02, 0x10 },
        11, { 0x06, 0x41, 0x07, 0x01, 0x01, 0x08, 0x00, 0x02, 0x07, 0x01, 0x1B }
    },
    {
        8, { 0x41, 0x05, 0x00, 0x01, 0x21, 0x10, 0x08, 0x3F },
        16, { 0x06, 0x41, 0x0D, 0x01, 0x01, 0x21, 0x10, 0x08, 0x28, 0xB0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
    },
    {
        5, { 0x01, 0xf7, 0x08, 0x00, 0x02 },
        2, { 0x07, 0x01 }
    },
    {
        8, { 0x41, 0x05, 0x00, 0x01, 0x08, 0x04, 0x02, 0x14 },
        11, { 0x06, 0x41, 0x07, 0x01, 0x01, 0x08, 0x04, 0x02, 0x00, 0x00, 0x17 }
    },
};

unsigned char input[100];
size_t inpidx = 0;

static void handle(int fd)
{
    unsigned char buf[1] = {0};
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
            ssize_t i = 0;
            while ( i < len ) {
                if (inpidx == sizeof(input)) {
                    logIT(LOG_ERR, "Simulator input buffer overflow avoided");
                    inpidx = 0;
                }
                input[inpidx] = buf[i];
                dump(&input[inpidx], 1, "received char:");
                inpidx++;
                int possible = 0;
                for (int j = 0; j < cmdc; j++) {
                    if (inpidx <= cmds[j].cmdlen &&
                            memcmp(cmds[j].cmd, input, inpidx) == 0) {
                        possible = 1;
                        if (cmds[j].cmdlen == inpidx) {
                            dump(input, inpidx, "received cmd:");
                            dump(cmds[j].rsp, cmds[j].rsplen, "answer:");
                            if (writen(fd, cmds[j].rsp, cmds[j].rsplen) !=
                                    (ssize_t)cmds[j].rsplen) {
                                printf("not completely written\n");
                            }
                            inpidx = 0;
                            break;
                        }
                    }
                }
                if (!possible && inpidx != 0) {
                    unsigned char last = input[inpidx - 1];
                    inpidx = 0;
                    for (int j = 0; j < cmdc; j++) {
                        if (cmds[j].cmd[0] == last) {
                            input[0] = last;
                            inpidx = 1;
                            break;
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
    int sockfd = -1;
    int listenfd = -1;

    (void)argc;
    (void)argv;
    listenfd = openSocket(SERVERPORT);
    while (1) {
        sockfd = listenToSocket(listenfd, makeDaemon);
        if (signal(SIGPIPE, sigPipeHandler) == SIG_ERR) {
            logIT(LOG_ERR, "Signal error");
            exit(1);
        }
        if (sockfd >= 0) {
            // Socket returned fd, the rest is done interactively
            logIT(LOG_INFO, "\nvcontrold connected");
            handle(sockfd);
            logIT(LOG_INFO, "\nvcontrold disconnected");
            closeSocket(sockfd);
        } else {
            logIT(LOG_ERR, "Error during connection setup");
        }
    }
}
