// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Communication with the vito controller

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include "io.h"
#include "socket.h"
#include "common.h"

#ifdef __CYGWIN__
// NCC is not defined under cygwin
#define NCC NCCS
#endif

void closeDevice(int fd)
{
    close(fd);
}

int openDevice(char *device)
{
    // We differentiate TTY and Socket connections.
    // Socker: no / at the beginning and a :
    int fd;
    char *dptr;

    if (device[0] != '/' && (dptr = strchr(device, ':'))) {
        char host[MAXBUF];
        int port;
        port = atoi(dptr + 1);
        // The dptr device gives us the length of the host
        memset(host, 0, sizeof(host));
        size_t hostlen = (size_t)(dptr - device);
        if (hostlen == 0 || hostlen >= sizeof(host)) {
            logIT1(LOG_ERR, "Invalid network device hostname");
            return -1;
        }
        memcpy(host, device, hostlen);
        host[hostlen] = '\0';
        // Third parameter == 1 --> noTCPDelay set
        fd = openCliSocket(host, port, 1);
    } else {
        fd = opentty(device);
    }

    if (fd < 0) {
        // Here goes some error stuff
        return -1;
    }

    return fd;
}

int opentty(char *device)
{
    int fd;

    logIT(LOG_INFO, "Configuring serial interface %s", device);
    if ((fd = open(device, O_RDWR)) < 0) {
        logIT(LOG_ERR, "cannot open %s:%m", device);
        return -1;
    }

    int s;
    struct termios oldsb, newsb;
    s = tcgetattr(fd, &oldsb);
    if (s < 0) {
        logIT(LOG_ERR, "error tcgetattr %s:%m", device);
        close(fd);
        return -1;
    }

    newsb = oldsb;

#ifdef NCC
    // NCC is not always defined
    for (s = 0; s < NCC; s++) {
        newsb.c_cc[s] = 0;
    }
#else
    memset(&newsb, 0, sizeof(newsb));
#endif

    newsb.c_iflag     = IGNBRK | IGNPAR;
    newsb.c_oflag     = 0;
    newsb.c_lflag     = 0; // removed ISIG for susp=control-z problem;
    newsb.c_cflag     = (CLOCAL | B4800 | CS8 | CREAD | PARENB | CSTOPB);
    newsb.c_cc[VMIN]  = 1;
    newsb.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSADRAIN, &newsb) < 0) {
        logIT(LOG_ERR, "error tcsetattr %s:%m", device);
        close(fd);
        return -1;
    }

    // DTR High for voltage supply
    int modemctl = 0;
    if (ioctl(fd, TIOCMGET, &modemctl) < 0) {
        logIT(LOG_ERR, "error ioctl TIOCMGET %s:%m", device);
        close(fd);
        return -1;
    }
    modemctl |= TIOCM_DTR;
    s = ioctl(fd, TIOCMSET, &modemctl);
    if (s < 0) {
        logIT(LOG_ERR, "error ioctl TIOCMSET %s:%m", device);
        close(fd);
        return -1;
    }

    return fd;
}

int my_send(int fd, char *s_buf, int len)
{
    int i;
    int wr;
    // Empty buffer
    if (isatty(fd) && tcflush(fd, TCIOFLUSH) < 0) {
        logIT(LOG_ERR, "Could not flush serial device: %s", strerror(errno));
        return 0;
    }
    // We use the socket fixed variant from socket.c
    wr = writen(fd, s_buf, len);
    for (i = 0; i < len; i++) {
        unsigned char byte = s_buf[i] & 255;
        logIT(LOG_INFO, ">SENT: %02X", (int)byte);
    }

    if (wr == len) {
        return wr;
    } else {
        logIT(LOG_ERR, ">ERROR: sent %d of %d bytes", wr, len);
        return 0;
    }
}

int receive(int fd, char *r_buf, int r_len, unsigned long *etime)
{
    return receive_nb(fd, r_buf, r_len, etime);
}

static char *dump(char *dest, size_t destSize, const char *title,
                  const char *buf, int len)
{
    size_t pos = 0;
    int i;

    if (!dest || destSize == 0) return dest;
    int written = snprintf(dest, destSize, "%s", title ? title : "");
    if (written < 0) {
        dest[0] = '\0';
        return dest;
    }
    pos = (size_t)written < destSize ? (size_t)written : destSize - 1;
    for (i = 0; i < len && pos < destSize - 1; i++) {
        written = snprintf(dest + pos, destSize - pos, " %02X",
                           (unsigned char)buf[i]);
        if (written < 0) break;
        if ((size_t)written >= destSize - pos) {
            pos = destSize - 1;
            break;
        }
        pos += (size_t)written;
    }

    return dest;
}

int receive_nb(int fd, char *r_buf, int r_len, unsigned long *etime)
{
    int i;
    ssize_t len;
    char string[MAXBUF * 3 + 64];

    fd_set rfds;
    struct timeval tv;
    int retval;

    struct tms tms_t;
    clock_t start, end, mid, mid1;
    unsigned long clktck;
    clktck = sysconf(_SC_CLK_TCK);
    start = times(&tms_t);
    mid1 = start;

    i = 0;
    while ( i < r_len ) {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = TIMEOUT;
        tv.tv_usec = 0;
        retval = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (retval == 0) {
            logIT(LOG_ERR, "<RECV: read timeout");
            logIT(LOG_INFO, "%s", dump(string, sizeof(string),
                                       "<RECV: received", r_buf, i));
            return -1;
        } else if (retval < 0) {
            if (errno == EINTR) {
                logIT(LOG_INFO, "<RECV: select interrupted - redo");
                continue;
            } else {
                logIT(LOG_ERR, "<RECV: select error %d", retval);
                logIT(LOG_INFO, "%s", dump(string, sizeof(string),
                                           "<RECV: received", r_buf, i));
                return -1;
            }
        } else if ( FD_ISSET(fd, &rfds)) {
            len = read(fd, &r_buf[i], r_len - i);
            if (len == 0) {
                logIT(LOG_ERR, "<RECV: read eof");
                logIT(LOG_INFO, "%s", dump(string, sizeof(string),
                                           "<RECV: received", r_buf, i));
                return -1;
            } else if (len < 0) {
                if (errno == EINTR) {
                    logIT(LOG_INFO, "<RECV: read interrupted - redo");
                    continue;
                } else {
                    logIT(LOG_ERR, "<RECV: read error %d", errno);
                    logIT(LOG_INFO, "%s", dump(string, sizeof(string),
                                               "<RECV: received", r_buf, i));
                    return -1;
                }
            } else {
                unsigned char byte = r_buf[i] & 255;
                mid = times(&tms_t);
                logIT(LOG_INFO, "<RECV: len=%zd %02X (%0.1f ms)", len, byte, ((float)(mid - mid1) / clktck) * 1000);
                mid1 = mid;
                i += len;
            }
        } else {
            continue;
        }
    }

    end = times(&tms_t);
    *etime = ((float)(end - start) / clktck) * 1000;
    logIT(LOG_INFO, "%s", dump(string, sizeof(string),
                               "<RECV: received", r_buf, i));

    return i;
}

int waitfor(int fd, char *w_buf, int w_len)
{
    int i;
    time_t start;
    char r_buf[MAXBUF];
    char hexString[128] = "\0";
    unsigned long etime;

    if (w_len < 1 || (size_t)w_len > (sizeof(hexString) - 1) / 2) {
        logIT(LOG_ERR, "Invalid wait sequence length: %d", w_len);
        return 0;
    }
    for (i = 0; i < w_len; i++) {
        snprintf(hexString + i * 2, sizeof(hexString) - (size_t)i * 2,
                 "%02X", (unsigned char)w_buf[i]);
    }

    logIT(LOG_INFO, "Waiting for %s", hexString);
    start = time(NULL);

    // We wait for the first character, then everything has to apply
    do {
        etime = 0;
        if (receive(fd, r_buf, 1, &etime) < 0) {
            return 0;
        }
        if (time(NULL) - start > TIMEOUT) {
            logIT1(LOG_WARNING, "Timeout wait");
            return 0;
        }
    } while (r_buf[0] != w_buf[0]);

    for (i = 1; i < w_len; i++) {
        etime = 0;
        if (receive(fd, r_buf, 1, &etime) < 0) {
            return 0;
        }
        if (time(NULL) - start > TIMEOUT) {
            logIT1(LOG_WARNING, "Timeout wait");
            return 0;
        }
        if ( r_buf[0] != w_buf[i]) {
            logIT1(LOG_ERR, "Lost synchronization");
            exit(1);
        }
    }

    // logIT1(LOG_INFO,"Recognized string");
    return 1;
}
