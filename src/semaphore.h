// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ipc.h>

#ifdef _CYGWIN__
#define TMPFILENAME "vcontrol.lockXXXXXX"
#else
#define TMPFILENAME "/tmp/vcontrol.lockXXXXXX"
#endif

extern char tmpfilename[MAXPATHLEN + 1]; // account for the leading '\0'

int initsem(key_t key, int nsems);
int vcontrol_seminit(void);
int vcontrol_semfree(void);
int vcontrol_semget(void);
int vcontrol_semrelease(void);

#endif // SEMAPHORE_H
