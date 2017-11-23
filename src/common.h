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

#ifndef COMMON_H
#define COMMON_H

int initLog(int useSyslog, char *logfile, int debugSwitch);
void logIT(int class, char *string, ...);
char hex2chr(char *hex);
int char2hex(char *outString, const char *charPtr, int len);
short string2chr(char *line, char *buf, short bufsize);
void sendErrMsg(int fd);
void setDebugFD(int fd);
ssize_t readn(int fd, void *vptr, size_t n);

#ifndef MAXBUF
#define MAXBUF 4096
#endif

#define logIT1(class, string) logIT(class, "%s", string)

#endif // COMMON_H
