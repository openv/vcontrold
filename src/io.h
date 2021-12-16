// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IO_H
#define IO_H

int my_send(int fd, char *s_buf, int len);
int receive(int fd, char *r_buf, int r_len, unsigned long *etime);
int receive_nb(int fd, char *r_buf, int r_len, unsigned long *etime);
int waitfor(int fd, char *w_buf, int w_len);
int opentty(char *device);
int openDevice(char *device);
void closeDevice(int fd);

// Interface parameters

#define TTY "/dev/usb/tts/0"

#ifndef MAXBUF
#define MAXBUF 4096
#endif
#define TIMEOUT 5

#endif // IO_H
