// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
