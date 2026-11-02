// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Common functions for vcontrold like logging and converting

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>

#include "common.h"

int syslogger = 0;
int debug = 0;
FILE *logFD;
char errMsg[2000];
int errClass = 99;
int dbgFD = -1;

int initLog(int useSyslog, const char *logfile, int debugSwitch)
{
    // If needed, opes the syslog or log file
    if (useSyslog) {
        syslogger = 1;
        openlog("vito", LOG_PID, LOG_LOCAL0);
        syslog(LOG_LOCAL0, "vito started");
    }

    if (logfile) {
        // Log file is not NULL and not empty
        if (strcmp(logfile, "") == 0) {
            return 0;
        }

        if (logFD) {
            fclose(logFD);
            logFD = NULL;
        }
        logFD = fopen(logfile, "a");
        if (! logFD) {
            printf("Could not open %s: %s", logfile, strerror (errno));
            return 0 ;
        }
    }

    debug = debugSwitch;
    memset(errMsg, 0, sizeof(errMsg));

    return 1;
}

void logIT(int class, const char *string, ...)
{
    va_list arguments;
    time_t t;
    char *tPtr;
    char *cPtr;
    time(&t);
    tPtr = ctime(&t);
    char *print_buffer = NULL;
    int pid;
    long avail;

    va_start(arguments, string);
    if (vasprintf(&print_buffer, string, arguments) < 0) {
        print_buffer = NULL;
    }
    va_end(arguments);
    if (!print_buffer) {
        return;
    }

    if (class <= LOG_ERR)  {
        avail = sizeof(errMsg) - strlen(errMsg) - 2;
        if ( avail > 0 ) {
            strncat(errMsg, print_buffer, avail);
            strcat(errMsg, "\n");
        } else {
            strcpy(&errMsg[sizeof(errMsg) - 12], "OVERFLOW\n");
            // Should solve the semop error
        }
    }

    errClass = class;
    // Remove control characters
    cPtr = tPtr;
    while (*cPtr) {
        if (iscntrl(*cPtr)) {
            *cPtr = ' ';
        }
        cPtr++;
    }

    if (dbgFD >= 0) {
        // The debug FD is set and we firstly send the info there
        dprintf(dbgFD, "DEBUG:%s: %s\n", tPtr, print_buffer);
    }

    if (! debug && (class  > LOG_NOTICE)) {
        free(print_buffer);
        return;
    }

    pid = getpid();

    if (syslogger) {
        syslog(class, "%s", print_buffer);
    }

    if (logFD) {
        fprintf(logFD, "[%d] %s: %s\n", pid, tPtr, print_buffer);
        fflush(logFD);
    }

    // Output only if 2 is open as STDERR
    if (isatty(2)) {
        fprintf(stderr, "[%d] %s: %s\n", pid, tPtr, print_buffer);
    }

    free(print_buffer);
}

void sendErrMsg(int fd)
{
    if ((fd >= 0) && (errClass <= 3)) {
        const char prefix[] = "ERR: ";
        ssize_t ignored;
        ignored = write(fd, prefix, sizeof(prefix) - 1);
        ignored = write(fd, errMsg, strlen(errMsg));
        (void)ignored;
        errClass = 99; // Thus it's only displayed once
        memset(errMsg, 0, sizeof(errMsg));
    }

    *errMsg = '\0';
    // Back to start, no matter if we actually output
    // Can be commented out for debugging, then we get the errors in errMsg
}

void setDebugFD(int fd)
{
    dbgFD = fd;
}

char hex2chr(const char *hex)
{
    char *end = NULL;
    unsigned long hex_value;
    if (!hex || !*hex) {
        return (char)-1;
    }
    errno = 0;
    hex_value = strtoul(hex, &end, 16);
    if (errno || end == hex || *end || hex_value > 0xff) {
        logIT(LOG_WARNING, "Invalid hex char in %s", hex);
        return (char)-1;
    }

    return (char)hex_value;
}

int char2hex(char *outString, size_t outSize, const char *charPtr, int len)
{
    int n;
    if (!outString || outSize == 0 || !charPtr || len <= 0) {
        if (outString) *outString = '\0';
        return 0;
    }
    if ((size_t)len > (outSize - 1) / 3) {
        *outString = '\0';
        logIT(LOG_ERR, "Hex output buffer too small for %d bytes", len);
        return 0;
    }
    for (n = 0; n < len; n++) {
        unsigned char byte = *charPtr++ & 255;
        snprintf(outString + (size_t)n * 3, outSize - (size_t)n * 3,
                 "%02X ", byte);
    }

    // Remove last space
    outString[strlen(outString) - 1] = '\0';

    return len;
}

short string2chr(char *line, char *buf, short bufsize)
{
    char *sptr, *saveptr = NULL;
    short count;

    count = 0;

    if (!line || !buf || bufsize <= 0) return -1;
    sptr = strtok_r(line, " \t\r\n", &saveptr);
    while (sptr && count < bufsize) {
        char value = hex2chr(sptr);
        if (value == (char)-1 && strcmp(sptr, "ff") && strcmp(sptr, "FF")) {
            return -1;
        }
        buf[count++] = value;
        sptr = strtok_r(NULL, " \t\r\n", &saveptr);
    }
    if (sptr) return -1;

    return count;
}

size_t stripLineEnding(char *line, size_t length)
{
    if (!line) return 0;
    while (length > 0 &&
            (line[length - 1] == '\r' || line[length - 1] == '\n')) {
        line[--length] = '\0';
    }
    return length;
}
