#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int main(void)
{
    char line[512];
    char bytes[MAXBUF] = {0};
    char unit[8] = {0};
    int length = 0;

    initLog(0, "/tmp/vcontrold-test-parser.log", 1);
    if (!test_bytecode_parser() || !test_expansion()) return 1;
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

    FILE *source = fopen("xml/300/vcontrold.xml", "rb");
    if (!source) return 1;
    if (fseek(source, 0, SEEK_END) != 0) return 1;
    long fileLength = ftell(source);
    if (fileLength <= 0 || fseek(source, 0, SEEK_SET) != 0) return 1;
    char *xml = calloc((size_t)fileLength + 1, 1);
    if (!xml || fread(xml, 1, (size_t)fileLength, source) !=
            (size_t)fileLength) return 1;
    fclose(source);
    char *debug = strstr(xml, "<debug>n</debug>");
    if (!debug) return 1;
    memcpy(debug, "<debug/>        ", strlen("<debug>n</debug>"));

    /*
     * Keep the temporary file beside the source configuration so its
     * relative XInclude paths continue to resolve.  The intended failure
     * must be the invalid boolean, not an unrelated missing include.
     */
    char temporary[] = "xml/300/.vcontrold-invalid-XXXXXX";
    int temporaryFd = mkstemp(temporary);
    if (temporaryFd < 0 ||
            write(temporaryFd, xml, (size_t)fileLength) != fileLength) return 1;
    close(temporaryFd);
    free(xml);
    int invalidAccepted = parseXMLFile(temporary);
    unlink(temporary);
    if (invalidAccepted || cfgPtr != validConfiguration) return 1;
    freeAllLists();
    return 0;
}
