#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "framer.h"

int inetversion = 0;

static unsigned char checksum(const unsigned char *frame, size_t total)
{
    unsigned char sum = 0;
    for (size_t i = 1; i + 1 < total; ++i) {
        sum = (unsigned char)(sum + frame[i]);
    }
    return sum;
}

static int receive_frame(const unsigned char *frame, size_t supplied,
                         int requested, unsigned char *result)
{
    int sockets[2];
    const unsigned char address[2] = {0x12, 0x34};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return -99;
    framer_test_set_p300_address(address);
    if (supplied > 0 &&
            write(sockets[0], frame, supplied) != (ssize_t)supplied) {
        close(sockets[0]);
        close(sockets[1]);
        return -99;
    }
    shutdown(sockets[0], SHUT_WR);
    unsigned long elapsed = 0;
    int rc = framer_receive(sockets[1], (char *)result, requested, &elapsed);
    close(sockets[0]);
    close(sockets[1]);
    return rc;
}

static int test_send(void)
{
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return 0;
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) {
        close(sockets[0]);
        unsigned char frame[16];
        if (read(sockets[1], frame, 8) != 8 || frame[0] != 0x41 ||
                frame[1] != 5 || frame[2] != 0x00 ||
                frame[3] != 0x01 || frame[4] != 0x12 ||
                frame[5] != 0x34 || frame[6] != 0x02) _exit(2);
        unsigned char ack = 0x06;
        if (write(sockets[1], &ack, 1) != 1) _exit(3);
        close(sockets[1]);
        _exit(0);
    }
    close(sockets[1]);
    const unsigned char address[2] = {0x12, 0x34};
    char payload[] = {0x00, 0x01, 0x12, 0x34, 0x02};
    framer_test_set_p300_address(address);
    int ok = framer_send(sockets[0], payload, sizeof(payload)) == 1;
    close(sockets[0]);
    int status = 0;
    ok = ok && waitpid(pid, &status, 0) == pid &&
        WIFEXITED(status) && WEXITSTATUS(status) == 0;

    framer_test_reset();
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return 0;
    char raw[] = {0x11, 0x22};
    ok = ok && framer_send(sockets[0], raw, sizeof(raw)) == 2;
    char received[2] = {0};
    ok = ok && read(sockets[1], received, sizeof(received)) == 2 &&
        memcmp(raw, received, sizeof(raw)) == 0;
    close(sockets[0]);
    close(sockets[1]);

    framer_test_set_p300_address(address);
    char tooShort[2] = {0};
    char tooLarge[254] = {0};
    return ok && framer_send(-1, NULL, 1) == 0 &&
        framer_send(-1, tooShort, sizeof(tooShort)) == 0 &&
        framer_send(-1, tooLarge, sizeof(tooLarge)) == 0;
}

int main(void)
{
    unsigned char result[8] = {0};
    unsigned char valid[] = {
        0x41, 0x07, 0x01, 0x01, 0x12, 0x34, 0x02, 0x55, 0xaa, 0x00
    };
    valid[sizeof(valid) - 1] = checksum(valid, sizeof(valid));

    initLog(0, "/tmp/vcontrold-test-framer.log", 1);
    if (!test_send()) return 1;

    for (size_t length = 0; length < sizeof(valid); ++length) {
        if (receive_frame(valid, length, 2, result) >= 1) {
            fprintf(stderr, "accepted truncated P300 frame of %zu bytes\n",
                    length);
            return 1;
        }
    }

    if (receive_frame(valid, sizeof(valid), 2, result) != 2 ||
            result[0] != 0x55 || result[1] != 0xaa) return 1;

    unsigned char malformed[sizeof(valid)];
    memcpy(malformed, valid, sizeof(valid));
    malformed[2] = 0x00;
    malformed[sizeof(malformed) - 1] = checksum(malformed, sizeof(malformed));
    if (receive_frame(malformed, sizeof(malformed), 2, result) >= 1) return 1;

    memcpy(malformed, valid, sizeof(valid));
    malformed[3] = 0x7f;
    malformed[sizeof(malformed) - 1] = checksum(malformed, sizeof(malformed));
    if (receive_frame(malformed, sizeof(malformed), 2, result) >= 1) return 1;

    memcpy(malformed, valid, sizeof(valid));
    malformed[4] = 0x13;
    malformed[sizeof(malformed) - 1] = checksum(malformed, sizeof(malformed));
    if (receive_frame(malformed, sizeof(malformed), 2, result) >= 1) return 1;

    memcpy(malformed, valid, sizeof(valid));
    malformed[sizeof(malformed) - 1] ^= 0xff;
    if (receive_frame(malformed, sizeof(malformed), 2, result) >= 1) return 1;

    memcpy(malformed, valid, sizeof(valid));
    malformed[6] = 0x03;
    malformed[sizeof(malformed) - 1] = checksum(malformed, sizeof(malformed));
    if (receive_frame(malformed, sizeof(malformed), 2, result) >= 1) return 1;

    unsigned char shortError[] = {
        0x41, 0x05, 0x03, 0x01, 0x12, 0x34, 0x00, 0x00
    };
    shortError[sizeof(shortError) - 1] =
        checksum(shortError, sizeof(shortError));
    if (receive_frame(shortError, sizeof(shortError), 1, result) >= 1) return 1;

    return 0;
}
