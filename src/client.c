// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Client routines for vcontrold queries

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include "client.h"
#include "prompt.h"
#include "common.h"
#include "socket.h"

int sendTrList(int sockfd, trPtr ptr);

trPtr newTrNode(trPtr ptr)
{
    trPtr nptr;

    if (ptr && ptr->next) {
        return newTrNode(ptr->next);
    }

    nptr = calloc(1, sizeof(*ptr));
    if (! nptr) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }

    if (ptr) {
        ptr->next = nptr;
    }

    return nptr;
}

ssize_t recvSync(int fd, char *wait, char **recv)
{
    char *rptr;
    char *pptr;
    char c;
    ssize_t count;
    int rcount = 1;

    *recv = NULL;
    if (! (*recv = calloc(ALLOCSIZE, sizeof(char)))) {
        logIT1(LOG_ERR, "calloc error");
        exit(1);
    }

    rptr = *recv;
    size_t i = 0;
    for (;;) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        int ready;
        do {
            ready = poll(&pfd, 1, CL_TIMEOUT * 1000);
        } while (ready < 0 && errno == EINTR);
        if (ready <= 0) {
            logIT(LOG_ERR, "timeout/error waiting for %s", wait);
            free(*recv);
            *recv = NULL;
            return -1;
        }
        count = readn(fd, &c, 1);
        if (count == 0) {
            break;
        }
        if (count < 0) {
            free(*recv);
            *recv = NULL;
            return -1;
        }

        if (i + 1 >= (size_t)ALLOCSIZE * rcount) {
            char *tmp = realloc(*recv, ALLOCSIZE * sizeof(char) *  ++rcount);
            if (tmp == NULL) {
                logIT1(LOG_ERR, "realloc error");
                exit(1);
            } else {
                *recv = tmp;
                rptr = *recv + i;
            }
        }
        *rptr++ = c;
        *rptr = '\0';
        i++;

        if ((pptr = strstr(*recv, wait))) {
            *pptr = '\0';
            logIT(LOG_INFO, "recv:%s", *recv);
            break;
        }

    }

    char *tmp = realloc(*recv, strlen(*recv) + 1);
    if (tmp == NULL) {
        logIT1(LOG_ERR, "realloc error");
        exit(1);
    } else {
        *recv = tmp;
    }

    if (count <= 0) {
        logIT(LOG_ERR, "exit with count=%ld", count);;
    }

    return count;
}

// port is never 0, which is a bad number for a tcp port
int connectServer(char *host, int port)
{
    int sockfd;

    if (host[0] != '/' ) {
        sockfd = openCliSocket(host, port, 0);
        if (sockfd) {
            logIT(LOG_INFO, "Setup connection to %s port %d", host, port);
        } else {
            logIT(LOG_INFO, "Setting up connection to %s port %d failed", host, port);
            return -1;
        }
    } else {
        logIT(LOG_ERR, "Host format: IP|Name:Port");
        return -1;
    }
    return sockfd;
}

void disconnectServer(int sockfd)
{
    char string[8];
    char *ptr;

    snprintf(string, sizeof(string), "quit\n");
    sendServer(sockfd, string, strlen(string));
    recvSync(sockfd, BYE, &ptr);
    free(ptr);
    close(sockfd);
}

size_t sendServer(int fd, char *s_buf, size_t len)
{
    char string[256];
    int flags = fcntl(fd, F_GETFL, 0);

    // Empty buffer
    // As tcflush does not work correctly, we use nonblocking read
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    while (readn(fd, string, sizeof(string)) > 0) { }
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags);
    }
    return Writen(fd, s_buf, len);
}

trPtr sendCmdFile(int sockfd, const char *filename)
{
    FILE *filePtr;
    char line[MAXBUF];
    trPtr ptr;
    trPtr startPtr = NULL;

    if (! (filePtr = fopen(filename, "r"))) {
        return NULL;
    } else {
        logIT(LOG_INFO, "Opened command file %s", filename);
    }

    memset(line, 0, sizeof(line));
    while (fgets(line, MAXBUF - 1, filePtr)) {
        ptr = newTrNode(startPtr);
        if (! startPtr) {
            startPtr = ptr;
        }
        size_t len = strcspn(line, "\r\n");
        ptr->cmd = calloc(len + 1, sizeof(char));
        if (!ptr->cmd) {
            fclose(filePtr);
            return NULL;
        }
        memcpy(ptr->cmd, line, len);
    }

    if (! sendTrList(sockfd, startPtr)) {
        // Something with the communication went wrong
        fclose(filePtr);
        return NULL;
    }

    fclose(filePtr);
    return startPtr;
}

trPtr sendCmds(int sockfd, char *commands)
{
    char *sptr;
    trPtr ptr;
    trPtr startPtr = NULL;

    if (!commands || !*commands) {
        logIT1(LOG_ERR, "Empty command list");
        return NULL;
    }
    sptr = strtok(commands, ",");
    while (sptr) {
        if (!*sptr) {
            sptr = strtok(NULL, ",");
            continue;
        }
        ptr = newTrNode(startPtr);
        if (! startPtr) {
            startPtr = ptr;
        }
        ptr->cmd = calloc(strlen(sptr) + 1, sizeof(char));
        if (!ptr->cmd) {
            logIT1(LOG_ERR, "calloc error");
            return NULL;
        }
        memcpy(ptr->cmd, sptr, strlen(sptr) + 1);
        sptr = strtok(NULL, ",");
    }

    if (!startPtr) {
        logIT1(LOG_ERR, "Empty command list");
        return NULL;
    }

    if (! sendTrList(sockfd, startPtr))
    {
        // Something with the communication went wrong
        return NULL;
    }

    return startPtr;
}

int sendTrList(int sockfd, trPtr ptr)
{
    char string[1000 + 1];
    char prompt[] = PROMPT;
    char errTXT[] = ERR;
    char *sptr;
    char *dumPtr;

    if (recvSync(sockfd, prompt, &sptr) <= 0) {
        free(sptr);
        return 0;
    }
    free(sptr);
    sptr = NULL;

    while (ptr) {
        //memset(string, 0,sizeof(string));
        snprintf(string, sizeof(string), "%s\n", ptr->cmd);

        if (sendServer(sockfd, string, strlen(string)) <= 0) {
            return 0;
        }

        //memset(string, 0,sizeof(string));
        logIT(LOG_INFO, "SEND:%s", ptr->cmd);
        if (recvSync(sockfd, prompt, &sptr) <= 0) {
            free(sptr);
            return 0;
        }

        ptr->raw = sptr;
        size_t rawlen = strlen(ptr->raw);
        if (rawlen > 0 && iscntrl((unsigned char)ptr->raw[rawlen - 1])) {
            ptr->raw[rawlen - 1] = '\0';
        }

        dumPtr = calloc(strlen(sptr) + 20, sizeof(char));
        snprintf(dumPtr, (strlen(sptr) + 20) * sizeof(char), "RECV:%s", sptr);
        logIT1(LOG_INFO, dumPtr);
        free(dumPtr);

        // We fill errors and result
        if (strstr(ptr->raw, errTXT) == ptr->raw) {
            ptr->err = ptr->raw;
            fprintf(stderr, "SRV %s\n", ptr->err);
        } else {
            // Here, we search the first word in raw and save it as result
            char *rptr;
            rptr = strchr(ptr->raw, ' ');
            if (! rptr) {
                rptr = ptr->raw + strlen(ptr->raw);
            }

            ptr->result = atof(ptr->raw);
            ptr->err = NULL;
        }

        ptr = ptr->next;
    }

    return 1;
}
