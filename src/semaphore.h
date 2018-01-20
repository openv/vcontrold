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

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <sys/param.h>

#ifdef _CYGWIN__
#define TMPFILENAME "vcontrol.lockXXXXXX"
#else
#define TMPFILENAME "/tmp/vcontrol.lockXXXXXX"
#endif

extern char tmpfilename[MAXPATHLEN + 1]; // account for the leading '\0'

int vcontrol_seminit();
int vcontrol_semfree();
int vcontrol_semget();
int vcontrol_semrelease();

#endif // SEMAPHORE_H
