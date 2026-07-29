#include <errno.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>

#include "common.h"
#include "semaphore.h"

int main(void)
{
    initLog(0, "/tmp/vcontrold-test-semaphore.log", 1);

    if (!vcontrol_seminit()) return 1;
    if (!vcontrol_semget()) return 1;
    if (!vcontrol_semrelease()) return 1;
    if (!vcontrol_semfree()) return 1;

    errno = 0;
    if (vcontrol_semget() != 0 || errno != EINVAL) return 1;
    if (vcontrol_semrelease() != 0 || errno != EINVAL) return 1;
    if (!vcontrol_semfree()) return 1;

    char filename[] = "/tmp/vcontrold-semaphore-unit-XXXXXX";
    int fd = mkstemp(filename);
    if (fd < 0 || close(fd) < 0) return 1;
    key_t key = ftok(filename, 'T');
    if (key == (key_t)-1) return 1;
    int first = initsem(key, 1);
    if (first < 0) return 1;
    int existing = initsem(key, 1);
    if (existing != first) return 1;
    errno = 0;
    if (initsem(key, 2) != -1 || errno != EINVAL) return 1;
    if (semctl(first, 0, IPC_RMID) < 0) return 1;
    unlink(filename);

    errno = 0;
    if (initsem(IPC_PRIVATE, -1) != -1 || errno == 0) return 1;
    return 0;
}
