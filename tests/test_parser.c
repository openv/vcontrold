#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"
#include "parser.h"

FILE *iniFD = NULL;
int inetversion = 0;
extern configPtr cfgPtr;
extern devicePtr devPtr;
void freeAllLists(void);

static int parse(const char *source, int expectedToken, int expectedLength)
{
    char line[512];
    char bytes[MAXBUF] = {0};
    char unit[100] = {0};
    int length = 0;
    snprintf(line, sizeof(line), "%s", source);
    int token = parseLine(line, bytes, &length, unit, sizeof(unit));
    return token == expectedToken && length == expectedLength;
}

static int test_bytecode_parser(void)
{
    char line[256];
    char bytes[MAXBUF] = {0};
    char unit[32] = {0};
    int length;

    struct {
        const char *line;
        int token;
        int length;
    } cases[] = {
        {"WAIT 05", WAIT, 1},
        {"SEND 01 F7 12 34", SEND, 4},
        {"SEND BYTES UT", BYTES, 1},
        {"RECV 0", RECV, 0},
        {"RECV 32767 UT", RECV, 32767},
        {"PAUSE 0", PAUSE, 0},
        {"UNKNOWN 1", 0, 0},
        {"RECV text", 0, 0},
        {"PAUSE 32768", 0, 0}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        snprintf(line, sizeof(line), "%s", cases[i].line);
        memset(bytes, 0, sizeof(bytes));
        memset(unit, 0, sizeof(unit));
        length = 0;
        if (parseLine(line, bytes, &length, unit, sizeof(unit)) !=
                cases[i].token || length != cases[i].length) return 0;
    }
    return 1;
}

static int test_expansion(void)
{
    Macro macro = {.name = "SYNC", .command = "SEND 04"};
    iCmd instruction = {
        .name = "getaddr",
        .send = "SYNC;SEND $addr $hexlen;RECV $len $unit"
    };
    Protocol protocol = {
        .name = "test", .mPtr = &macro, .icPtr = &instruction
    };
    Unit unit = {.name = "UT", .type = "short"};
    Command command = {
        .name = "getTest", .pcmd = "getaddr", .addr = "1234",
        .unit = "UT", .len = 2
    };
    if (!expand(&command, &protocol) ||
            strcmp(command.send, "SEND 04;SEND 12 34 02;RECV 2 UT") != 0 ||
            !buildByteCode(&command, &unit)) return 0;
    removeCompileList(command.cmpPtr);
    free(command.send);

    instruction.send = "SEND $unknown";
    command.send = NULL;
    command.cmpPtr = NULL;
    return expand(&command, &protocol) == 0;
}

static int test_exec_bytecode_transport(void)
{
    int sockets[2];
    char received[64] = {0};
    char parameter[8] = {0};

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return 0;
    char sendBytes[] = {0x10, 0x20};
    char response[] = {0x33, 0x44};
    Compile recvNode = {.token = RECV, .len = 2};
    Compile sendNode = {
        .token = SEND, .send = sendBytes, .len = 2, .next = &recvNode
    };
    if (write(sockets[1], response, sizeof(response)) !=
            (ssize_t)sizeof(response) ||
            execByteCode(&sendNode, sockets[0], received, sizeof(received),
                         parameter, sizeof(parameter), 0, 1, 0, 1, NULL, 0) != 2) return 0;
    char sent[2] = {0};
    if (read(sockets[1], sent, sizeof(sent)) != (ssize_t)sizeof(sent) ||
            memcmp(sent, sendBytes, sizeof(sent)) != 0 ||
            memcmp(received, response, sizeof(response)) != 0) return 0;
    close(sockets[0]);
    close(sockets[1]);

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return 0;
    char waitByte = 0x05;
    char afterWait = 0x06;
    Compile sendAfterWait = {
        .token = SEND, .send = &afterWait, .len = 1
    };
    Compile waitNode = {
        .token = WAIT, .send = &waitByte, .len = 1, .next = &sendAfterWait
    };
    if (write(sockets[1], &waitByte, 1) != 1 ||
            execByteCode(&waitNode, sockets[0], received, sizeof(received),
                         parameter, sizeof(parameter), 0, 1, 0, 1, NULL, 0) != 0 ||
            read(sockets[1], sent, 1) != 1 || sent[0] != afterWait) return 0;
    close(sockets[0]);
    close(sockets[1]);

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return 0;
    char forwarded[] = {0x55, 0x66};
    Compile bytesNode = {.token = BYTES, .len = 2};
    if (execByteCode(&bytesNode, sockets[0], received, sizeof(received),
                     forwarded, sizeof(forwarded), sizeof(forwarded), 1, 0, 1, NULL, 0) != 0 ||
            read(sockets[1], sent, 2) != 2 ||
            memcmp(sent, forwarded, 2) != 0) return 0;
    free(bytesNode.send);
    close(sockets[0]);
    close(sockets[1]);

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return 0;
    char wrong = 0x7f;
    char good = 0x42;
    char replies[] = {wrong, good};
    Compile retryNode = {
        .token = RECV, .len = 1, .errStr = &wrong
    };
    if (write(sockets[1], replies, sizeof(replies)) !=
            (ssize_t)sizeof(replies) ||
            execByteCode(&retryNode, sockets[0], received, sizeof(received),
                         parameter, sizeof(parameter), 0, 1, 0, 2, NULL, 0) != 1 ||
            received[0] != good) return 0;
    close(sockets[0]);
    close(sockets[1]);

    Compile invalid = {.token = 99};
    return execByteCode(&invalid, -1, received, sizeof(received),
                        parameter, sizeof(parameter), 0, 1, 0, 1, NULL, 0) == -1 &&
        execByteCode(NULL, -1, received, sizeof(received),
                     parameter, sizeof(parameter), 0, 1, 0, 1, NULL, 0) == -1;
}

static int test_exec_command_transport(void)
{
    int sockets[2];
    char received[8] = {0};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return 0;
    char sendLine[] = "SEND AA 55";
    if (execCmd(sendLine, sockets[0], received, sizeof(received)) != 0) return 0;
    char bytes[2] = {0};
    if (read(sockets[1], bytes, 2) != 2 ||
            (unsigned char)bytes[0] != 0xaa ||
            (unsigned char)bytes[1] != 0x55) return 0;
    char reply[] = {0x11, 0x22};
    if (write(sockets[1], reply, sizeof(reply)) != (ssize_t)sizeof(reply))
        return 0;
    char recvLine[] = "RECV 2";
    if (execCmd(recvLine, sockets[0], received, sizeof(received)) != 2 ||
            memcmp(received, reply, sizeof(reply)) != 0) return 0;
    char pauseLine[] = "PAUSE 0";
    char unknownLine[] = "NOOP";
    int ok = execCmd(pauseLine, sockets[0], received, sizeof(received)) == 0 &&
        execCmd(unknownLine, sockets[0], received, sizeof(received)) == 0;
    close(sockets[0]);
    close(sockets[1]);
    return ok;
}

static int test_exec_bytecode_units_and_failures(void)
{
    int sockets[2];
    char received[64] = {0};
    char previous[8] = {0};
    char parameter[32] = {0};
    Unit getUnit = {
        .name = "get", .type = "uchar", .gCalc = "V*2"
    };
    Compile getNode = {.token = RECV, .len = 1, .uPtr = &getUnit};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return 0;
    char value = 4;
    if (write(sockets[1], &value, 1) != 1 ||
            execByteCode(&getNode, sockets[0], received, sizeof(received),
                         parameter, sizeof(parameter), 0, 0, 0, 1, previous, 0) != 0 ||
            strncmp(received, "8.000000", 8) != 0) return 0;
    close(sockets[0]);
    close(sockets[1]);

    Unit setUnit = {
        .name = "set", .type = "uchar", .sCalc = "V"
    };
    Compile setNode = {.token = BYTES, .len = 1, .uPtr = &setUnit};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return 0;
    snprintf(parameter, sizeof(parameter), "7");
    if (execByteCode(&setNode, sockets[0], received, sizeof(received),
                     parameter, sizeof(parameter), 1, 0, 0, 1, previous, 0) != 0) return 0;
    char sent = 0;
    if (read(sockets[1], &sent, 1) != 1 || sent != 7) return 0;
    free(setNode.send);
    close(sockets[0]);
    close(sockets[1]);

    char sendByte = 1;
    Compile sendFailure = {
        .token = SEND, .send = &sendByte, .len = 1
    };
    if (execByteCode(&sendFailure, -1, received, sizeof(received),
                     parameter, sizeof(parameter), 0, 1, 0, 1, previous, 0) != -1) return 0;

    Compile waitFailure = {
        .token = WAIT, .send = &sendByte, .len = 1
    };
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return 0;
    close(sockets[1]);
    if (execByteCode(&waitFailure, sockets[0], received, sizeof(received),
                     parameter, sizeof(parameter), 0, 1, 0, 1, previous, 0) != -1) return 0;
    close(sockets[0]);

    Compile receiveFailure = {.token = RECV, .len = 1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return 0;
    close(sockets[1]);
    if (execByteCode(&receiveFailure, sockets[0], received, sizeof(received),
                     parameter, sizeof(parameter), 0, 1, 0, 1, previous, 0) != -1) return 0;
    close(sockets[0]);

    Compile wrongLength = {.token = BYTES, .len = 2};
    snprintf(parameter, sizeof(parameter), "x");
    if (execByteCode(&wrongLength, -1, received, sizeof(received),
                     parameter, sizeof(parameter), 1, 1, 0, 1, previous, 0) != -1) return 0;

    char oversizedData[1025] = {0};
    Compile oversized = {
        .token = SEND, .send = oversizedData, .len = sizeof(oversizedData)
    };
    if (execByteCode(&oversized, -1, received, sizeof(received),
                     parameter, sizeof(parameter), 0, 1, 0, 1, previous, 0) != -1) return 0;

    Compile pause = {.token = PAUSE, .len = 0};
    return execByteCode(&pause, -1, received, sizeof(received),
                        parameter, sizeof(parameter), 0, 1, 0, 1, previous, 0) == 0;
}

static int test_simulation_output(void)
{
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) return 0;
    char sendByte = 0x11;
    char response = 0x22;
    char received[8] = {0};
    char parameter[2] = {0};
    Compile recvNode = {.token = RECV, .len = 1};
    Compile sendNode = {
        .token = SEND, .send = &sendByte, .len = 1, .next = &recvNode
    };
    iniFD = tmpfile();
    if (!iniFD || write(sockets[1], &response, 1) != 1) return 0;
    int rc = execByteCode(&sendNode, sockets[0], received, sizeof(received),
                          parameter, sizeof(parameter), 0, 1, 0, 1, NULL, 0);
    long outputLength = -1;
    if (fflush(iniFD) == 0 && fseek(iniFD, 0, SEEK_END) == 0) {
        outputLength = ftell(iniFD);
    }
    fclose(iniFD);
    iniFD = NULL;
    close(sockets[0]);
    close(sockets[1]);
    return rc == 1 && outputLength > 0;
}

static int rejected_modified_config(const char *needle,
                                    const char *replacement,
    configPtr expectedConfiguration)
{
    FILE *source = fopen("xml/300/vcontrold.xml", "rb");
    if (!source) return 0;
    if (fseek(source, 0, SEEK_END) != 0) {
        fclose(source);
        return 0;
    }
    long sourceLength = ftell(source);
    if (sourceLength <= 0 || fseek(source, 0, SEEK_SET) != 0) {
        fclose(source);
        return 0;
    }
    char *xml = calloc((size_t)sourceLength + 1, 1);
    if (!xml || fread(xml, 1, (size_t)sourceLength, source) !=
            (size_t)sourceLength) {
        free(xml);
        fclose(source);
        return 0;
    }
    fclose(source);

    char *match = strstr(xml, needle);
    if (!match) {
        free(xml);
        return 0;
    }
    size_t prefixLength = (size_t)(match - xml);
    size_t needleLength = strlen(needle);
    size_t replacementLength = strlen(replacement);
    size_t outputLength = (size_t)sourceLength - needleLength +
        replacementLength;
    char *output = calloc(outputLength + 1, 1);
    if (!output) {
        free(xml);
        return 0;
    }
    memcpy(output, xml, prefixLength);
    memcpy(output + prefixLength, replacement, replacementLength);
    memcpy(output + prefixLength + replacementLength,
           match + needleLength,
           (size_t)sourceLength - prefixLength - needleLength);
    free(xml);

    char temporary[] = "xml/300/.vcontrold-invalid-XXXXXX";
    int temporaryFd = mkstemp(temporary);
    int ok = temporaryFd >= 0 &&
        write(temporaryFd, output, outputLength) == (ssize_t)outputLength;
    if (temporaryFd >= 0) close(temporaryFd);
    free(output);
    if (!ok) {
        unlink(temporary);
        return 0;
    }
    int accepted = parseXMLFile(temporary);
    unlink(temporary);
    return !accepted && cfgPtr == expectedConfiguration;
}

int main(void)
{
    char line[512];
    char bytes[MAXBUF] = {0};
    char unit[8] = {0};
    int length = 0;

    signal(SIGPIPE, SIG_IGN);
    initLog(0, "/tmp/vcontrold-test-parser.log", 1);
    if (!test_bytecode_parser() || !test_expansion() ||
            !test_exec_bytecode_transport() ||
            !test_exec_command_transport() ||
            !test_exec_bytecode_units_and_failures() ||
            !test_simulation_output()) return 1;
    if (!parse("RECV 2 UT", RECV, 2)) return 1;
    if (!parse("PAUSE 100", PAUSE, 100)) return 1;
    if (!parse("RECV -1", 0, 0)) return 1;
    if (!parse("RECV 999999999999", 0, 0)) return 1;

    snprintf(line, sizeof(line), "RECV 2 unit-name-that-is-too-long");
    if (parseLine(line, bytes, &length, unit, sizeof(unit)) != 0) return 1;
    if (unit[sizeof(unit) - 1] != '\0') return 1;

    if (!parseXMLFile("xml/kw/vcontrold.xml")) return 1;
    if (!cfgPtr || !cfgPtr->devPtr ||
            strcmp(cfgPtr->devPtr->id, "2053") != 0) return 1;

    if (!parseXMLFile("xml/300/vcontrold.xml")) return 1;
    devicePtr excludedDevice = getDeviceNode(devPtr, "2053");
    devicePtr supportedDevice = getDeviceNode(devPtr, "20CB");
    commandPtr excludedCommand = excludedDevice ?
        getCommandNode(excludedDevice->cmdPtr, "getTempVListM1") : NULL;
    commandPtr supportedCommand = supportedDevice ?
        getCommandNode(supportedDevice->cmdPtr, "getTempVListM1") : NULL;
    if (!excludedCommand || excludedCommand->addr) return 1;
    if (!supportedCommand || !supportedCommand->addr ||
            strcmp(supportedCommand->addr, "2900") != 0) return 1;
    configPtr validConfiguration = cfgPtr;
    struct {
        const char *needle;
        const char *replacement;
    } invalidConfigurations[] = {
        {"<debug>n</debug>", "<debug/>"},
        {"<debug>n</debug>", "<debug>maybe</debug>"},
        {"<syslog>n</syslog>", "<syslog>invalid</syslog>"},
        {"<port>3002</port>", "<port>70000</port>"},
        {"<device ID=\"20CB\"/>", "<device ID=\"FFFF\"/>"},
        {"<V-Control xmlns:vcontrol=", "<X-Control xmlns:vcontrol="},
        {"http://www.openv.de/vcontrol",
         "http://www.openv.de/not-vcontrol"},
        {"href=\"vito.xml\"", "href=\"missing-vito.xml\""},
        {"<macro name=\"SYNC\">", "<macro name=\"MISSING_SYNC\">"},
        {"<command name=\"getaddr\">",
         "<command name=\"missing_getaddr\">"}
    };
    for (size_t i = 0;
            i < sizeof(invalidConfigurations) /
                sizeof(invalidConfigurations[0]);
            ++i) {
        if (!rejected_modified_config(invalidConfigurations[i].needle,
                                      invalidConfigurations[i].replacement,
                                      validConfiguration)) return 1;
    }
    if (!parseXMLFile("xml/20c8/vcontrold.xml") ||
            !cfgPtr || !cfgPtr->devPtr ||
            strcmp(cfgPtr->devPtr->id, "20C8") != 0 ||
            !getCommandNode(cfgPtr->devPtr->cmdPtr, "getTempA") ||
            !getCommandNode(cfgPtr->devPtr->cmdPtr, "setTempWWSoll")) {
        return 1;
    }
    freeAllLists();
    return 0;
}
