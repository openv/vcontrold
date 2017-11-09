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

int parseLine(char *lineo, char *hex, int *hexlen, char *uSPtr, ssize_t uSPtrLen);
int execCmd(char *cmd, int fd, char *result, int resultLen);
void removeCompileList(compilePtr ptr);
int execByteCode(compilePtr cmpPtr, int fd, char *recvBuf, short recvLen, char *sendBuf, short sendLen, short supressUnit, char bitpos, int retry, char *pRecvPtr, unsigned short recvTimeout);
void compileCommand(devicePtr dPtr, unitPtr uPtr);

/* Token Definition */

#define WAIT    1
#define RECV    2
#define SEND    3
#define PAUSE   4
#define BYTES   5

#ifndef MAXBUF
#define MAXBUF 4096
#endif
