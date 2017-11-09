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

#define CL_TIMEOUT 25

#ifndef MAXBUF
#define MAXBUF 4096
#endif

#define ALLOCSIZE 256

typedef struct txRx *trPtr;

ssize_t recvSync(int fd, char *wait, char **recv);
int connectServer(char *host, int port);
void disconnectServer(int sockfd);
size_t sendServer(int fd, char *s_buf, size_t len);
trPtr sendCmdFile(int sockfd, const char *tmpfile);
trPtr sendCmds(int sockfd, char *commands);

struct txRx {
    char *cmd;
    float result;
    char *err;
    char *raw;
    time_t timestamp;
    trPtr next;
} TxRx;
