#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "client.h"
#include "prompt.h"

int inetversion = 0;

static void free_results(trPtr ptr)
{
    while (ptr) {
        trPtr next = ptr->next;
        free(ptr->cmd);
        free(ptr->raw);
        free(ptr);
        ptr = next;
    }
}

static int start_server(const char *const responses[], size_t count,
                        int *clientFd, pid_t *serverPid)
{
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return 0;
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) {
        close(sockets[0]);
        if (write(sockets[1], PROMPT, strlen(PROMPT)) < 0) _exit(2);
        for (size_t i = 0; i < count; ++i) {
            char command[1200];
            size_t used = 0;
            while (used + 1 < sizeof(command)) {
                ssize_t got = read(sockets[1], command + used, 1);
                if (got <= 0) _exit(3);
                if (command[used++] == '\n') break;
            }
            if (write(sockets[1], responses[i], strlen(responses[i])) < 0 ||
                    write(sockets[1], PROMPT, strlen(PROMPT)) < 0) _exit(4);
        }
        close(sockets[1]);
        _exit(0);
    }
    close(sockets[1]);
    *clientFd = sockets[0];
    *serverPid = pid;
    return 1;
}

static int finish_server(int fd, pid_t pid)
{
    int status = 0;
    close(fd);
    return waitpid(pid, &status, 0) == pid &&
        WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int test_command_list(void)
{
    const char *responses[] = {"12.5 unit\n", "ERR: unavailable\n"};
    int fd;
    pid_t pid;
    if (!start_server(responses, 2, &fd, &pid)) return 0;
    char commands[] = ",temperature,,status,";
    trPtr result = sendCmds(fd, commands);
    int ok = result && result->next && !result->next->next &&
        strcmp(result->cmd, "temperature") == 0 &&
        result->result == 12.5 && result->err == NULL &&
        strcmp(result->next->cmd, "status") == 0 &&
        result->next->err == result->next->raw;
    free_results(result);
    return finish_server(fd, pid) && ok;
}

static int test_command_file(void)
{
    char filename[] = "/tmp/vcontrold-client-XXXXXX";
    int fileFd = mkstemp(filename);
    if (fileFd < 0) return 0;
    const char content[] = "first\r\nsecond";
    if (write(fileFd, content, sizeof(content) - 1) !=
            (ssize_t)(sizeof(content) - 1)) return 0;
    close(fileFd);

    const char *responses[] = {"1\n", "2\n"};
    int fd;
    pid_t pid;
    if (!start_server(responses, 2, &fd, &pid)) return 0;
    trPtr result = sendCmdFile(fd, filename);
    unlink(filename);
    int ok = result && result->next && !result->next->next &&
        strcmp(result->cmd, "first") == 0 &&
        strcmp(result->next->cmd, "second") == 0;
    free_results(result);
    return finish_server(fd, pid) && ok;
}

int main(void)
{
    char empty[] = "";
    char commas[] = ",,,";
    if (sendCmds(-1, empty) != NULL || sendCmds(-1, commas) != NULL) {
        fputs("empty command list was accepted\n", stderr);
        return 1;
    }
    if (!test_command_list()) return 1;
    if (!test_command_file()) return 1;

    const char *responses[] = {""};
    int fd;
    pid_t pid;
    if (!start_server(responses, 0, &fd, &pid)) return 1;
    char *received = NULL;
    if (recvSync(fd, PROMPT, &received) <= 0 ||
            !received || *received != '\0') return 1;
    free(received);
    if (!finish_server(fd, pid)) return 1;
    return 0;
}
