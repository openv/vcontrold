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

/*
 * Framer interface
 *
 * For P300 framing is supported here
 *
 * Control is by a controlling byte defining the action
 *
 * 2013-01-31 vheat
 */

/*
 * Main indentification of P300 Protocol is about leadin 0x41
 */

#ifndef FRAMER_H
#define FRAMER_H

#define FRAMER_ERROR    0
#define FRAMER_SUCCESS  1

int framer_send(int fd, char *s_buf, int len);
int framer_waitfor(int fd, char *w_buf, int w_len);
int framer_receive(int fd, char *r_buf, int r_len, unsigned long *petime);
int framer_openDevice(char *device, char pid);
void framer_closeDevice(int fd);

#endif // FRAMER_H
