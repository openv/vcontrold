// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
    unsigned short *array;
};
#endif

/*
 * initsem() -- more-than-inspired by W. Richard Stevens' UNIX Network
 * Programming 2nd edition, volume 2, lockvsem.c, page 295.
 */
int initsem(key_t key, int nsems)
{
    // key from ftok()
    int i;
    union semun arg;
    struct semid_ds buf;
    struct sembuf sb;
    int semid;

    semid = semget(key, nsems, IPC_CREAT | IPC_EXCL | 0660);

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
            // error, check errno
            return semid;
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

int semid = -1;
char tmpfilename[MAXPATHLEN + 1]; // account for the leading '\0'

int vcontrol_seminit(void)
{
    key_t key;
    int tmpfile;

    snprintf(tmpfilename, sizeof(tmpfilename), "%s", TMPFILENAME);
    if ((tmpfile = mkstemp(tmpfilename)) < 0) {
        logIT(LOG_ERR, "mkstemp failed: %s", strerror(errno));
        return 0;
    }
    if (close(tmpfile) < 0) {
        int saved_errno = errno;
        unlink(tmpfilename);
        errno = saved_errno;
        logIT(LOG_ERR, "close semaphore key file failed: %s", strerror(errno));
        return 0;
    }

    if ((key = ftok(tmpfilename, 'V')) == -1) {
        logIT(LOG_ERR, "ftok failed: %s", strerror(errno));
        unlink(tmpfilename);
        tmpfilename[0] = '\0';
        return 0;
    }

    // grab the semaphore set created by seminit.c:
    if ((semid = initsem(key, 1)) == -1) {
        logIT(LOG_ERR, "initsem failed: %s", strerror(errno));
        unlink(tmpfilename);
        tmpfilename[0] = '\0';
        return 0;
    }
    return 1;
}

int vcontrol_semfree(void)
{
    // remove it:
    if (semid >= 0 && semctl(semid, 0, IPC_RMID) == -1 &&
            errno != EINVAL && errno != EIDRM) {
        logIT(LOG_ERR, "semctl failed: %s", strerror(errno));
        return 0;
    }
    semid = -1;
    if (tmpfilename[0] != '\0' && unlink(tmpfilename) < 0 && errno != ENOENT) {
        logIT(LOG_ERR, "unlink semaphore key file failed: %s", strerror(errno));
        return 0;
    }
    tmpfilename[0] = '\0';
    return 1; // what should we return?
}

int vcontrol_semget(void)
{
    logIT(LOG_INFO, "Process %d tries to aquire lock", getpid());

    struct sembuf sb;

    sb.sem_num = 0;
    sb.sem_op = -1; // set to allocate resource
    sb.sem_flg = SEM_UNDO;
    while (semop(semid, &sb, 1) == -1) {
        if (errno == EINTR) {
            continue;
        }
        perror("semop");
        return 0;
    }
    logIT(LOG_INFO, "Process %d got lock", getpid());
    return 1;
}

int vcontrol_semrelease(void)
{
    struct sembuf sb;

    logIT(LOG_INFO, "Process %d released lock", getpid());

    sb.sem_num = 0;
    sb.sem_op = 1; // free resource
    sb.sem_flg = SEM_UNDO;
    while (semop(semid, &sb, 1) == -1) {
        if (errno == EINTR) {
            continue;
        }
        perror("semop");
        return 0;
    }

    return 1;
}
