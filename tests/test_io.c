#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"
#include "io.h"

int inetversion = 0;

static int test_receive_and_send(void)
{
    int sockets[2];
    char buffer[4] = {0};
    unsigned long elapsed = 0;
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return 0;

    const char outgoing[] = {0x01, 0x02, 0x03};
    if (my_send(sockets[0], outgoing, sizeof(outgoing)) !=
            (int)sizeof(outgoing) ||
            receive(sockets[1], buffer, sizeof(outgoing), &elapsed) !=
            (int)sizeof(outgoing) ||
            memcmp(buffer, outgoing, sizeof(outgoing)) != 0) {
        close(sockets[0]);
        close(sockets[1]);
        return 0;
    }

    close(sockets[0]);
    int ok = receive_nb(sockets[1], buffer, 1, &elapsed) == -1;
    close(sockets[1]);
    return ok;
}

static int test_waitfor(void)
{
    int sockets[2];
    unsigned char expected[] = {0x04, 0x05};
    unsigned char matching[] = {0x00, 0x04, 0x05};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return 0;
    int ok = write(sockets[0], matching, sizeof(matching)) ==
        (ssize_t)sizeof(matching) &&
        waitfor(sockets[1], (const char *)expected, sizeof(expected)) == 1;
    close(sockets[0]);
    close(sockets[1]);
    if (!ok || socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return 0;

    unsigned char mismatch[] = {0x04, 0x06};
    ok = write(sockets[0], mismatch, sizeof(mismatch)) ==
        (ssize_t)sizeof(mismatch) &&
        waitfor(sockets[1], (const char *)expected, sizeof(expected)) == 0;
    close(sockets[0]);
    close(sockets[1]);
    return ok;
}

int main(void)
{
    initLog(0, "/tmp/vcontrold-test-io.log", 1);
    char byte = 0;
    unsigned long elapsed = 0;

    if (openDevice(NULL) != -1 ||
            openDevice("") != -1 ||
            openDevice("localhost:0") != -1 ||
            openDevice("localhost:not-a-port") != -1 ||
            openDevice(":3002") != -1 ||
            opentty("/definitely/not/a/device") != -1 ||
            opentty("/dev/null") != -1) return 1;

    errno = 0;
    if (receive_nb(-1, &byte, 1, &elapsed) != -1 || errno != EINVAL ||
            receive_nb(0, NULL, 1, &elapsed) != -1 ||
            receive_nb(0, &byte, 0, &elapsed) != -1 ||
            receive_nb(0, &byte, 1, NULL) != -1 ||
            waitfor(-1, &byte, 0) != 0 ||
            waitfor(-1, &byte, 64) != 0 ||
            my_send(-1, &byte, 1) != 0) return 1;

    closeDevice(-1);
    return (!test_receive_and_send() || !test_waitfor()) ? 1 : 0;
}
