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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <string.h>
#include <syslog.h>
#include <sys/param.h>

#include "common.h"
#include "semaphore.h"

#define MAX_RETRIES 10

#if !defined(__APPLE__)
// on my mac this is a redefinition
union semun {
    int val;
    struct semid_ds *buf;
    ushort *array;
};
#endif

/*
** initsem() -- more-than-inspired by W. Richard Stevens' UNIX Network
** Programming 2nd edition, volume 2, lockvsem.c, page 295.
*/
int initsem(key_t key, int nsems)
{
    //key from ftok()
    int i;
    union semun arg;
    struct semid_ds buf;
    struct sembuf sb;
    int semid;

    semid = semget(key, nsems, IPC_CREAT | IPC_EXCL | 0666);

    if (semid >= 0) { // we got it first
        sb.sem_op = 1;
        sb.sem_flg = 0;
        arg.val = 1;

        for (sb.sem_num = 0; sb.sem_num < nsems; sb.sem_num++) {
            // do a semop() to "free" the semaphores.
            // this sets the sem_otime field, as needed below.
            if (semop(semid, &sb, 1) == -1) {
                int e = errno;
                semctl(semid, 0, IPC_RMID); // clean up
                errno = e;
                return -1; // error, check errno
            }
        }
    } else if (errno == EEXIST) {
        // someone else got it first
        int ready = 0;

        semid = semget(key, nsems, 0); // get the id
        if (semid < 0) {
            return semid;
            // error, check errno
        }

        // wait for other process to initialize the semaphore:
        arg.buf = &buf;
        for (i = 0; i < MAX_RETRIES && !ready; i++) {
            semctl(semid, nsems - 1, IPC_STAT, arg);
            if (arg.buf->sem_otime != 0) {
                ready = 1;
            } else {
                sleep(1);
            }
        }
        if (! ready) {
            errno = ETIME;
            return -1;
        }
    } else {
        return semid;
        // error, check errno
    }

    return semid;
}

#ifdef _CYGWIN__
#define TMPFILENAME "vcontrol.lockXXXXXX"
#else
#define TMPFILENAME "/tmp/vcontrol.lockXXXXXX"
#endif

char tmpfilename[MAXPATHLEN + 1]; // account for the leading '\0'
int semid;

int vcontrol_seminit()
{
    key_t key;
    struct sembuf sb;

    int tmpfile;

    strncpy(tmpfilename, TMPFILENAME, sizeof(TMPFILENAME));
    if ((tmpfile = mkstemp(tmpfilename)) < 0) {
        perror("mkstemp");
        exit(1);
    }
    close(tmpfile);

    sb.sem_num = 0;
    sb.sem_op = -1; // set to allocate resource
    sb.sem_flg = SEM_UNDO;

    if ((key = ftok(tmpfilename, 'V')) == -1) {
        perror("ftok");
        exit(1);
    }

    // grab the semaphore set created by seminit.c:
    if ((semid = initsem(key, 1)) == -1) {
        perror("initsem");
        exit(1);
    }
    return 1;
}

int vcontrol_semfree()
{
    // remove it:
    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl");
        exit(1);
    }
    unlink(tmpfilename);
    return 1; // what should we return?
}

int vcontrol_semget()
{
    logIT(LOG_INFO, "Process %d tries to aquire lock", getpid());

    struct sembuf sb;

    sb.sem_num = 0;
    sb.sem_op = -1; // set to allocate resource
    sb.sem_flg = SEM_UNDO;
    if (semop(semid, &sb, 1) == -1) {
        perror("semop");
        exit(1);
    }
    logIT(LOG_INFO, "Process %d got lock", getpid());
    return 1;
}

int vcontrol_semrelease()
{
    struct sembuf sb;

    logIT(LOG_INFO, "Process %d released lock", getpid());

    sb.sem_num = 0;
    sb.sem_op = 1; // free resource
    sb.sem_flg = SEM_UNDO;
    if (semop(semid, &sb, 1) == -1) {
        perror("semop");
        exit(1);
    }

    return 1;
}
