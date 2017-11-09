/*  Copyright 2007-2017 the original vcontrold development team

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef IO_H
#define IO_H

int my_send(int fd, char *s_buf, int len);
int receive(int fd, char *r_buf, int r_len, unsigned long *etime);
int receive_nb(int fd, char *r_buf, int r_len, unsigned long *etime);
int waitfor(int fd, char *w_buf, int w_len);
int opentty(char *device);
int openDevice(char *device);
void closeDevice(int fd);

/* Schnittstellenparameter */

#define TTY    "/dev/usb/tts/0"

#ifndef MAXBUF
#define MAXBUF 4096
#endif
#define TIMEOUT 5

#endif // IO_H
